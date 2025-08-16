// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QGENERICITEMMODEL_IMPL_H
#define QGENERICITEMMODEL_IMPL_H

#ifndef Q_QDOC

#ifndef QGENERICITEMMODEL_H
#error Do not include qgenericitemmodel_impl.h directly
#endif

#if 0
#pragma qt_sync_skip_header_check
#pragma qt_sync_stop_processing
#endif

#include <QtCore/qabstractitemmodel.h>
#include <QtCore/qmetaobject.h>
#include <QtCore/qvariant.h>

#include <functional>
#include <iterator>
#include <type_traits>
#include <QtCore/q20type_traits.h>
#include <tuple>
#include <QtCore/q23utility.h>

QT_BEGIN_NAMESPACE

namespace QGenericItemModelDetails
{
    template <typename T, template <typename...> typename... Templates>
    struct is_any_of_impl : std::false_type {};

    template <template <typename...> typename Template,
              typename... Params,
              template <typename...> typename... Templates>
    struct is_any_of_impl<Template<Params...>, Template, Templates...> : std::true_type {};

    template <typename T,
              template <typename...> typename Template,
              template <typename...> typename... Templates>
    struct is_any_of_impl<T, Template, Templates...> : is_any_of_impl<T, Templates...> {};

    template <typename T, template <typename...> typename... Templates>
    using is_any_of = is_any_of_impl<std::remove_cv_t<T>, Templates...>;

    template <typename T>
    using is_validatable = std::is_constructible<bool, T>;

    template <typename T, typename = void>
    struct is_smart_ptr : std::false_type {};

    template <typename T>
    struct is_smart_ptr<T,
        std::enable_if_t<std::conjunction_v<
                std::is_pointer<decltype(std::declval<T&>().get())>,
                std::is_same<decltype(*std::declval<T&>().get()), decltype(*std::declval<T&>())>,
                is_validatable<T>
            >>>
        : std::true_type
    {};

    // TODO: shouldn't we check is_smart_ptr && !is_copy_constructible && !is_copy_assignable
    //       to support users-specific ptrs?
    template <typename T>
    using is_any_unique_ptr = is_any_of<T, std::unique_ptr, QScopedPointer>;

    template <typename T>
    using is_any_shared_ptr = is_any_of<T, std::shared_ptr, QSharedPointer,
                                           QExplicitlySharedDataPointer, QSharedDataPointer>;

    template <typename T>
    using is_owning_or_raw_pointer = std::disjunction<is_any_shared_ptr<T>, is_any_unique_ptr<T>, std::is_pointer<T>>;


    template <typename T>
    static auto pointerTo(T&& t) {
        using Type = q20::remove_cvref_t<T>;
        if constexpr (is_any_of<Type, std::optional>())
            return t ? std::addressof(*std::forward<T>(t)) : nullptr;
        else if constexpr (std::is_pointer<Type>())
            return t;
        else if constexpr (is_smart_ptr<Type>())
            return t.get();
        else if constexpr (is_any_of<Type, std::reference_wrapper>())
            return std::addressof(t.get());
        else
            return std::addressof(std::forward<T>(t));
    }

    template <typename T>
    using wrapped_t = std::remove_pointer_t<decltype(pointerTo(std::declval<T&>()))>;

    template <typename T>
    using is_wrapped = std::negation<std::is_same<wrapped_t<T>, std::remove_reference_t<T>>>;

    template <typename T>
    static constexpr bool isValid(const T &t) noexcept
    {
        if constexpr (is_validatable<T>())
            return bool(t);
        else
            return true;
    }

    template <typename T>
    static decltype(auto) refTo(T&& t) {
        Q_ASSERT(isValid(t));
        // it's allowed to move only if the object holds unique ownership of the wrapped data
        using Type = q20::remove_cvref_t<T>;
        if constexpr (is_any_of<T, std::optional>())
            return *std::forward<T>(t); // let std::optional resolve dereferencing
        if constexpr (!is_wrapped<Type>() || is_any_unique_ptr<Type>())
            return q23::forward_like<T>(*pointerTo(t));
        else
            return *pointerTo(t);
    }

    template <typename It>
    auto key(It&& it) -> decltype(it.key()) { return it.key(); }
    template <typename It>
    auto key(It&& it) -> decltype((it->first)) { return it->first; }

    template <typename It>
    auto value(It&& it) -> decltype(it.value()) { return it.value(); }
    template <typename It>
    auto value(It&& it) -> decltype((it->second)) { return it->second; }

    // use our own version of begin/end so that we can overload for pointers
    template <typename C>
    static auto begin(C &&c) -> decltype(std::begin(refTo(std::forward<C>(c))))
    { return std::begin(refTo(std::forward<C>(c))); }
    template <typename C>
    static auto end(C &&c) -> decltype(std::end(refTo(std::forward<C>(c))))
    { return std::end(refTo(std::forward<C>(c))); }
    template <typename C>
    static auto cbegin(C &&c) -> decltype(std::cbegin(refTo(std::forward<C>(c))))
    { return std::cbegin(refTo(std::forward<C>(c))); }
    template <typename C>
    static auto cend(C &&c) -> decltype(std::cend(refTo(std::forward<C>(c))))
    { return std::cend(refTo(std::forward<C>(c))); }
    template <typename C>
    static auto pos(C &&c, int i)
    { return std::next(QGenericItemModelDetails::begin(std::forward<C>(c)), i); }
    template <typename C>
    static auto cpos(C &&c, int i)
    { return std::next(QGenericItemModelDetails::cbegin(std::forward<C>(c)), i); }

    // Test if a type is a range, and whether we can modify it using the
    // standard C++ container member functions insert, erase, and resize.
    // For the sake of QAIM, we cannot modify a range if it holds const data
    // even if the range itself is not const; we'd need to initialize new rows
    // and columns, and move old row and column data.
    template <typename C, typename = void>
    struct test_insert : std::false_type {};

    template <typename C>
    struct test_insert<C, std::void_t<decltype(std::declval<C>().insert(
        std::declval<typename C::const_iterator>(),
        std::declval<typename C::size_type>(),
        std::declval<typename C::value_type>()
    ))>>
        : std::true_type
    {};

    // Can we insert from another (identical) range? Required to support
    // move-only types
    template <typename C, typename = void>
    struct test_insert_range : std::false_type {};

    template <typename C>
    struct test_insert_range<C, std::void_t<decltype(std::declval<C&>().insert(
      std::declval<typename C::const_iterator&>(),
      std::declval<std::move_iterator<typename C::iterator>&>(),
      std::declval<std::move_iterator<typename C::iterator>&>()
    ))>>
        : std::true_type
    {};

    template <typename C, typename = void>
    struct test_erase : std::false_type {};

    template <typename C>
    struct test_erase<C, std::void_t<decltype(std::declval<C>().erase(
        std::declval<typename C::const_iterator>(),
        std::declval<typename C::const_iterator>()
    ))>>
        : std::true_type
    {};

    template <typename C, typename = void>
    struct test_resize : std::false_type {};

    template <typename C>
    struct test_resize<C, std::void_t<decltype(std::declval<C>().resize(
        std::declval<typename C::size_type>(),
        std::declval<typename C::value_type>()
    ))>>
        : std::true_type
    {};

    // Test if a type is an associative container that we can use for multi-role
    // data, i.e. has a key_type and a mapped_type typedef, and maps from int,
    // Qt::ItemDataRole, or QString to QVariant. This excludes std::set (and
    // unordered_set), which are not useful for us anyway even though they are
    // considered associative containers.
    template <typename C, typename = void> struct is_multi_role : std::false_type
    {
        static constexpr bool int_key = false;
    };
    template <typename C> // Qt::ItemDataRole -> QVariant, or QString -> QVariant, int -> QVariant
    struct is_multi_role<C, std::void_t<typename C::key_type, typename C::mapped_type>>
        : std::conjunction<std::disjunction<std::is_same<typename C::key_type, int>,
                                            std::is_same<typename C::key_type, Qt::ItemDataRole>,
                                            std::is_same<typename C::key_type, QString>>,
                           std::is_same<typename C::mapped_type, QVariant>>
    {
        static constexpr bool int_key = !std::is_same_v<typename C::key_type, QString>;
    };
    template <typename C>
    [[maybe_unused]]
    static constexpr bool is_multi_role_v = is_multi_role<C>::value;

    template <typename C, typename = void>
    struct test_size : std::false_type {};
    template <typename C>
    struct test_size<C, std::void_t<decltype(std::size(std::declval<C&>()))>> : std::true_type {};

    template <typename C, typename = void>
    struct range_traits : std::false_type {
        static constexpr bool is_mutable = !std::is_const_v<C>;
        static constexpr bool has_insert = false;
        static constexpr bool has_insert_range = false;
        static constexpr bool has_erase = false;
        static constexpr bool has_resize = false;
    };
    template <typename C>
    struct range_traits<C, std::void_t<decltype(cbegin(std::declval<C&>())),
                                       decltype(cend(std::declval<C&>())),
                                       std::enable_if_t<!is_multi_role_v<C>>
                                      >> : std::true_type
    {
        using iterator = decltype(begin(std::declval<C&>()));
        using value_type = std::remove_reference_t<decltype(*std::declval<iterator&>())>;
        static constexpr bool is_mutable = !std::is_const_v<C> && !std::is_const_v<value_type>;
        static constexpr bool has_insert = test_insert<C>();
        static constexpr bool has_insert_range = test_insert_range<C>();
        static constexpr bool has_erase = test_erase<C>();
        static constexpr bool has_resize = test_resize<C>();
    };

    // Specializations for types that look like ranges, but should be
    // treated as values.
    enum class Mutable { Yes, No };
    template <Mutable IsMutable>
    struct iterable_value : std::false_type {
        static constexpr bool is_mutable = IsMutable == Mutable::Yes;
        static constexpr bool has_insert = false;
        static constexpr bool has_erase = false;
        static constexpr bool has_resize = false;
    };
    template <> struct range_traits<QByteArray> : iterable_value<Mutable::Yes> {};
    template <> struct range_traits<QString> : iterable_value<Mutable::Yes> {};
    template <class CharT, class Traits, class Allocator>
    struct range_traits<std::basic_string<CharT, Traits, Allocator>> : iterable_value<Mutable::Yes>
    {};

    // const T * and views are read-only
    template <typename T> struct range_traits<const T *> : iterable_value<Mutable::No> {};
    template <> struct range_traits<QLatin1StringView> : iterable_value<Mutable::No> {};

    template <typename C>
    [[maybe_unused]] static constexpr bool is_range_v = range_traits<C>();

    // Find out how many fixed elements can be retrieved from a row element.
    // main template for simple values and ranges. Specializing for ranges
    // is ambiguous with arrays, as they are also ranges
    template <typename T, typename = void>
    struct row_traits {
        static constexpr bool is_range = is_range_v<q20::remove_cvref_t<T>>;
        // A static size of -1 indicates dynamically sized range
        // A static size of 0 indicates that the specified type doesn't
        // represent static or dynamic range.
        static constexpr int static_size = is_range ? -1 : 0;
        static constexpr int fixed_size() { return 1; }
    };

    // Specialization for tuples, using std::tuple_size
    template <typename T>
    struct row_traits<T, std::void_t<std::tuple_element_t<0, T>>> {
        static constexpr std::size_t size64 = std::tuple_size_v<T>;
        static_assert(q20::in_range<int>(size64));
        static constexpr int static_size = int(size64);
        static constexpr int fixed_size() { return 0; }
    };

    // Specialization for C arrays
    template <typename T, std::size_t N>
    struct row_traits<T[N]>
    {
        static_assert(q20::in_range<int>(N));
        static constexpr int static_size = int(N);
        static constexpr int fixed_size() { return 0; }
    };

    // Specialization for gadgets
    // clang doesn't accept multiple specializations using std::void_t directly
    template <class... Ts> struct make_void { using type = void; };

    template <typename T>
    struct row_traits<T, typename make_void<decltype(T::staticMetaObject)>::type>
    {
        static constexpr int static_size = 0;
        static int fixed_size() {
            // Interpret a gadget in a list as a multi-column row item. To
            // disambiguate, stick it into a SingleColumn wrapper.
            static const int columnCount = []{
                const QMetaObject &mo = T::staticMetaObject;
                return mo.propertyCount() - mo.propertyOffset();
            }();
            return columnCount;
        }
    };

    template <typename T>
    [[maybe_unused]] static constexpr int static_size_v =
                            row_traits<std::remove_cv_t<wrapped_t<T>>>::static_size;

    template <typename Range>
    struct ListProtocol
    {
        using row_type = typename range_traits<wrapped_t<Range>>::value_type;

        template <typename R = row_type>
        auto newRow() -> decltype(R{}) { return R{}; }
    };

    template <typename Range>
    struct TableProtocol
    {
        using row_type = typename range_traits<wrapped_t<Range>>::value_type;

        template <typename R = row_type,
                  std::enable_if_t<std::is_destructible_v<wrapped_t<R>>, bool> = true,
                  std::enable_if_t<is_owning_or_raw_pointer<R>::value, bool> = true>
        auto newRow() -> decltype(R(new wrapped_t<R>))
        {
            if constexpr (is_any_of<R, std::shared_ptr>())
                return std::make_shared<wrapped_t<R>>();
            else
                return R(new wrapped_t<R>);
        }

        template <typename R = row_type,
                  std::enable_if_t<std::negation_v<is_wrapped<R>>, bool> = true,
                  std::enable_if_t<std::negation_v<is_validatable<R>>, bool> = true>
        auto newRow() -> decltype(R{}) { return R{}; }

        template <typename R,
                  std::enable_if_t<std::is_pointer_v<R>, bool> = true>
        auto deleteRow(R row) -> decltype(delete(row)) { delete row; }
    };

    template <typename Range, typename R = typename range_traits<wrapped_t<Range>>::value_type>
    using table_protocol_t = std::conditional_t<static_size_v<R> == 0,
                                                ListProtocol<Range>, TableProtocol<Range>>;

    // Default tree traversal protocol implementation for row types that have
    // the respective member functions. The trailing return type implicitly
    // removes those functions that are not available.
    template <typename Range>
    struct DefaultTreeProtocol : TableProtocol<Range>
    {
        template <typename R /*wrapped_row_type*/>
        auto parentRow(const R& row) const -> decltype(row.parentRow())
        {
            return row.parentRow();
        }

        template <typename R /* = wrapped_row_type*/>
        auto setParentRow(R &row, R* parent) -> decltype(row.setParentRow(parent))
        {
            row.setParentRow(parent);
        }

        template <typename R /* = wrapped_row_type*/>
        auto childRows(const R &row) const -> decltype(row.childRows())
        {
            return row.childRows();
        }

        template <typename R /* = wrapped_row_type*/>
        auto childRows(R &row) -> decltype(row.childRows())
        {
            return row.childRows();
        }
    };

    template <typename P, typename R, typename = void>
    struct protocol_parentRow : std::false_type {};
    template <typename P, typename R>
    struct protocol_parentRow<P, R,
            std::void_t<decltype(std::declval<P&>().parentRow(std::declval<wrapped_t<R>&>()))>>
        : std::true_type {};

    template <typename P, typename R, typename = void>
    struct protocol_childRows : std::false_type {};
    template <typename P, typename R>
    struct protocol_childRows<P, R,
            std::void_t<decltype(std::declval<P&>().childRows(std::declval<wrapped_t<R>&>()))>>
        : std::true_type {};

    template <typename P, typename R, typename = void>
    struct protocol_setParentRow : std::false_type {};
    template <typename P, typename R>
    struct protocol_setParentRow<P, R,
            std::void_t<decltype(std::declval<P&>().setParentRow(std::declval<wrapped_t<R>&>(),
                                                                 std::declval<wrapped_t<R>*>()))>>
        : std::true_type {};

    template <typename P, typename R, typename = void>
    struct protocol_mutable_childRows : std::false_type {};
    template <typename P, typename R>
    struct protocol_mutable_childRows<P, R,
            std::void_t<decltype(refTo(std::declval<P&>().childRows(std::declval<wrapped_t<R>&>()))
                                                                                            = {}) >>
        : std::true_type {};

    template <typename P, typename = void>
    struct protocol_newRow : std::false_type {};
    template <typename P>
    struct protocol_newRow<P, std::void_t<decltype(std::declval<P&>().newRow())>>
        : std::true_type {};

    template <typename P, typename R, typename = void>
    struct protocol_deleteRow : std::false_type {};
    template <typename P, typename R>
    struct protocol_deleteRow<P, R,
            std::void_t<decltype(std::declval<P&>().deleteRow(std::declval<R&&>()))>>
        : std::true_type {};

    template <typename Range,
              typename Protocol = DefaultTreeProtocol<Range>,
              typename R = typename range_traits<Range>::value_type,
              typename = void>
    struct is_tree_range : std::false_type {};

    template <typename Range, typename Protocol, typename R>
    struct is_tree_range<Range, Protocol, R,
                         std::enable_if_t<std::conjunction_v<
                            protocol_parentRow<Protocol, R>, protocol_childRows<Protocol, R>>>
            > : std::true_type {};

    template <typename Range>
    using if_is_table_range = std::enable_if_t<is_range_v<wrapped_t<Range>>
                                            && !is_tree_range<wrapped_t<Range>>(),
                                               bool>;

    template <typename Range, typename Protocol = DefaultTreeProtocol<Range>>
    using if_is_tree_range = std::enable_if_t<is_range_v<wrapped_t<Range>>
                                           && is_tree_range<wrapped_t<Range>, wrapped_t<Protocol>>(),
                                              bool>;

    template <typename Range, typename Protocol>
    struct protocol_traits
    {
        using protocol = wrapped_t<Protocol>;
        using row = typename range_traits<wrapped_t<Range>>::value_type;

        static constexpr bool has_newRow = protocol_newRow<protocol>();
        static constexpr bool has_deleteRow = protocol_deleteRow<protocol, row>();
        static constexpr bool has_setParentRow = protocol_setParentRow<protocol, row>();
        static constexpr bool has_mutable_childRows = protocol_mutable_childRows<protocol, row>();

        static constexpr bool initializes_rows = has_newRow && !is_any_of<protocol, ListProtocol>();
        static constexpr bool is_default = is_any_of<protocol, ListProtocol, TableProtocol, DefaultTreeProtocol>();
    };

    // The storage of the model data. We might store it as a pointer, or as a
    // (copied- or moved-into) value (or smart pointer). But we always return a
    // raw pointer.
    template <typename ModelStorage>
    struct ModelData
    {
        auto model() { return pointerTo(m_model); }
        auto model() const { return pointerTo(m_model); }

        ModelStorage m_model;
    };
}

class QGenericItemModel;

class QGenericItemModelImplBase
{
    Q_DISABLE_COPY_MOVE(QGenericItemModelImplBase)
protected:
    // Helpers for calling a lambda with the tuple element at a runtime index.
    template <typename Tuple, typename F, size_t ...Is>
    static void call_at(Tuple &&tuple, size_t idx, std::index_sequence<Is...>, F &&function)
    {
        if (QGenericItemModelDetails::isValid(tuple))
            ((Is == idx ? static_cast<void>(function(get<Is>(
                            QGenericItemModelDetails::refTo(std::forward<Tuple>(tuple)))))
                        : static_cast<void>(0)), ...);
    }

    template <typename T, typename F>
    static auto for_element_at(T &&tuple, size_t idx, F &&function)
    {
        using type = QGenericItemModelDetails::wrapped_t<T>;
        constexpr size_t size = std::tuple_size_v<type>;
        Q_ASSERT(idx < size);
        return call_at(std::forward<T>(tuple), idx, std::make_index_sequence<size>{},
                       std::forward<F>(function));
    }

    // Get the QMetaType for a tuple-element at a runtime index.
    // Used in the headerData implementation.
    template <typename Tuple, std::size_t ...I>
    static constexpr std::array<QMetaType, sizeof...(I)> makeMetaTypes(std::index_sequence<I...>)
    {
        return {{QMetaType::fromType<q20::remove_cvref_t<std::tuple_element_t<I, Tuple>>>()...}};
    }
    template <typename T>
    static constexpr QMetaType meta_type_at(size_t idx)
    {
        using type = QGenericItemModelDetails::wrapped_t<T>;
        constexpr auto size = std::tuple_size_v<type>;
        Q_ASSERT(idx < size);
        return makeMetaTypes<type>(std::make_index_sequence<size>{}).at(idx);
    }

    // Helpers to call a given member function with the correct arguments.
    template <typename Class, typename T, typename F, size_t...I>
    static auto apply(std::integer_sequence<size_t, I...>, Class* obj, F&& fn, T&& tuple)
    {
        return std::invoke(fn, obj, std::get<I>(tuple)...);
    }
    template <typename Ret, typename Class, typename ...Args>
    static void makeCall(QGenericItemModelImplBase *obj, Ret(Class::* &&fn)(Args...),
                              void *ret, const void *args)
    {
        const auto &tuple = *static_cast<const std::tuple<Args&...> *>(args);
        *static_cast<Ret *>(ret) = apply(std::make_index_sequence<sizeof...(Args)>{},
                                         static_cast<Class *>(obj), fn, tuple);
    }
    template <typename Ret, typename Class, typename ...Args>
    static void makeCall(const QGenericItemModelImplBase *obj, Ret(Class::* &&fn)(Args...) const,
                         void *ret, const void *args)
    {
        const auto &tuple = *static_cast<const std::tuple<Args&...> *>(args);
        *static_cast<Ret *>(ret) = apply(std::make_index_sequence<sizeof...(Args)>{},
                                         static_cast<const Class *>(obj), fn, tuple);
    }

public:
    enum ConstOp {
        Index,
        Parent,
        Sibling,
        RowCount,
        ColumnCount,
        Flags,
        HeaderData,
        Data,
        ItemData,
    };

    enum Op {
        Destroy,
        SetData,
        SetItemData,
        ClearItemData,
        InsertColumns,
        RemoveColumns,
        MoveColumns,
        InsertRows,
        RemoveRows,
        MoveRows,
    };

    void destroy()
    {
        call<bool>(Destroy);
    }

private:
    // prototypes
    static void callConst(ConstOp, const QGenericItemModelImplBase *, void *, const void *);
    static void call(Op, QGenericItemModelImplBase *, void *, const void *);

    using CallConstFN = decltype(callConst);
    using CallTupleFN = decltype(call);

    CallConstFN *callConst_fn;
    CallTupleFN *call_fn;

protected:
    template <typename Impl> // type deduction
    explicit QGenericItemModelImplBase(QGenericItemModel *itemModel, const Impl *)
        : callConst_fn(&Impl::callConst), call_fn(&Impl::call), m_itemModel(itemModel)
    {}
    ~QGenericItemModelImplBase() = default;

    QGenericItemModel *m_itemModel;

    inline QModelIndex createIndex(int row, int column, const void *ptr = nullptr) const;
    inline void changePersistentIndexList(const QModelIndexList &from, const QModelIndexList &to);
    inline QHash<int, QByteArray> roleNames() const;
    inline void dataChanged(const QModelIndex &from, const QModelIndex &to,
                            const QList<int> &roles);
    inline void beginInsertColumns(const QModelIndex &parent, int start, int count);
    inline void endInsertColumns();
    inline void beginRemoveColumns(const QModelIndex &parent, int start, int count);
    inline void endRemoveColumns();
    inline bool beginMoveColumns(const QModelIndex &sourceParent, int sourceFirst, int sourceLast,
                                 const QModelIndex &destParent, int destRow);
    inline void endMoveColumns();
    inline void beginInsertRows(const QModelIndex &parent, int start, int count);
    inline void endInsertRows();
    inline void beginRemoveRows(const QModelIndex &parent, int start, int count);
    inline void endRemoveRows();
    inline bool beginMoveRows(const QModelIndex &sourceParent, int sourceFirst, int sourceLast,
                              const QModelIndex &destParent, int destRow);
    inline void endMoveRows();

public:
    template <typename Ret, typename ...Args>
    Ret callConst(ConstOp op, const Args &...args) const
    {
        Ret ret = {};
        const auto tuple = std::tie(args...);
        callConst_fn(op, this, &ret, &tuple);
        return ret;
    }

    template <typename Ret, typename ...Args>
    Ret call(Op op, const Args &...args)
    {
        Ret ret = {};
        const auto tuple = std::tie(args...);
        call_fn(op, this, &ret, &tuple);
        return ret;
    }
};

QT_END_NAMESPACE

#endif // Q_QDOC

#endif // QGENERICITEMMODEL_IMPL_H
