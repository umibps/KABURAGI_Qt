// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QGENERICITEMMODEL_H
#define QGENERICITEMMODEL_H

#include <QtCore/qgenericitemmodel_impl.h>
#include <QtCore/qmap.h>

#include <algorithm>

QT_BEGIN_NAMESPACE

class Q_CORE_EXPORT QGenericItemModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    template <typename T>
    using SingleColumn = std::tuple<T>;

    template <typename T>
    struct MultiColumn
    {
        using type = std::remove_pointer_t<T>;
        T data{};
        template <typename X>
        using if_get_matches = std::enable_if_t<std::is_same_v<q20::remove_cvref_t<X>,
                                                               MultiColumn<T>>, bool>;

        template <typename V = T,
                  std::enable_if_t<QGenericItemModelDetails::is_validatable<V>::value, bool> = true>
        constexpr explicit operator bool() const noexcept { return bool(data); }

        // unconstrained on size_t I, gcc internal error #3280
        template <std::size_t I, typename V, if_get_matches<V> = true>
        friend inline decltype(auto) get(V &&multiColumn)
        {
            static_assert(I < std::tuple_size_v<type>, "Index out of bounds for wrapped type");
            return get<I>(QGenericItemModelDetails::refTo(q23::forward_like<V>(multiColumn.data)));
        }
    };

    template <typename Range,
              QGenericItemModelDetails::if_is_table_range<Range> = true>
    explicit QGenericItemModel(Range &&range, QObject *parent = nullptr);

    template <typename Range,
              QGenericItemModelDetails::if_is_tree_range<Range> = true>
    explicit QGenericItemModel(Range &&range, QObject *parent = nullptr);

    template <typename Range, typename Protocol,
              QGenericItemModelDetails::if_is_tree_range<Range, Protocol> = true>
    explicit QGenericItemModel(Range &&range, Protocol &&protocol, QObject *parent = nullptr);

    ~QGenericItemModel() override;

    QModelIndex index(int row, int column, const QModelIndex &parent = {}) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    QModelIndex sibling(int row, int column, const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &data, int role = Qt::EditRole) override;
    QMap<int, QVariant> itemData(const QModelIndex &index) const override;
    bool setItemData(const QModelIndex &index, const QMap<int, QVariant> &data) override;
    bool clearItemData(const QModelIndex &index) override;
    bool insertColumns(int column, int count, const QModelIndex &parent = {}) override;
    bool removeColumns(int column, int count, const QModelIndex &parent = {}) override;
    bool moveColumns(const QModelIndex &sourceParent, int sourceColumn, int count,
                     const QModelIndex &destParent, int destColumn) override;
    bool insertRows(int row, int count, const QModelIndex &parent = {}) override;
    bool removeRows(int row, int count, const QModelIndex &parent = {}) override;
    bool moveRows(const QModelIndex &sourceParent, int sourceRow, int count,
                  const QModelIndex &destParent, int destRow) override;

    bool canFetchMore(const QModelIndex &parent) const override;
    void fetchMore(const QModelIndex &parent) override;

    bool hasChildren(const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex buddy(const QModelIndex &index) const override;
    bool canDropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column,
                         const QModelIndex &parent) const override;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column,
                      const QModelIndex &parent) override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
    QStringList mimeTypes() const override;
    QModelIndexList match(const QModelIndex &start, int role, const QVariant &value, int hits,
                          Qt::MatchFlags flags) const override;
    void multiData(const QModelIndex &index, QModelRoleDataSpan roleDataSpan) const override;
    QHash<int, QByteArray> roleNames() const override;
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;
    QSize span(const QModelIndex &index) const override;
    Qt::DropActions supportedDragActions() const override;
    Qt::DropActions supportedDropActions() const override;

protected Q_SLOTS:
    void resetInternalData() override;

protected:
    bool event(QEvent *) override;
    bool eventFilter(QObject *, QEvent *) override;

private:
    Q_DISABLE_COPY_MOVE(QGenericItemModel)

    friend class QGenericItemModelImplBase;
    struct Deleter { void operator()(QGenericItemModelImplBase *that) { that->destroy(); } };
    std::unique_ptr<QGenericItemModelImplBase, Deleter> impl;
};

// implementation of forwarders
QModelIndex QGenericItemModelImplBase::createIndex(int row, int column, const void *ptr) const
{
    return m_itemModel->createIndex(row, column, ptr);
}
void QGenericItemModelImplBase::changePersistentIndexList(const QModelIndexList &from,
                                                          const QModelIndexList &to)
{
    m_itemModel->changePersistentIndexList(from, to);
}
QHash<int, QByteArray> QGenericItemModelImplBase::roleNames() const
{
    return m_itemModel->roleNames();
}
void QGenericItemModelImplBase::dataChanged(const QModelIndex &from, const QModelIndex &to,
                                            const QList<int> &roles)
{
    m_itemModel->dataChanged(from, to, roles);
}
void QGenericItemModelImplBase::beginInsertColumns(const QModelIndex &parent, int start, int count)
{
    m_itemModel->beginInsertColumns(parent, start, count);
}
void QGenericItemModelImplBase::endInsertColumns()
{
    m_itemModel->endInsertColumns();
}
void QGenericItemModelImplBase::beginRemoveColumns(const QModelIndex &parent, int start, int count)
{
    m_itemModel->beginRemoveColumns(parent, start, count);
}
void QGenericItemModelImplBase::endRemoveColumns()
{
    m_itemModel->endRemoveColumns();
}
bool QGenericItemModelImplBase::beginMoveColumns(const QModelIndex &sourceParent, int sourceFirst,
                                                 int sourceLast, const QModelIndex &destParent,
                                                 int destColumn)
{
    return m_itemModel->beginMoveColumns(sourceParent, sourceFirst, sourceLast,
                                         destParent, destColumn);
}
void QGenericItemModelImplBase::endMoveColumns()
{
    m_itemModel->endMoveColumns();
}

void QGenericItemModelImplBase::beginInsertRows(const QModelIndex &parent, int start, int count)
{
    m_itemModel->beginInsertRows(parent, start, count);
}
void QGenericItemModelImplBase::endInsertRows()
{
    m_itemModel->endInsertRows();
}
void QGenericItemModelImplBase::beginRemoveRows(const QModelIndex &parent, int start, int count)
{
    m_itemModel->beginRemoveRows(parent, start, count);
}
void QGenericItemModelImplBase::endRemoveRows()
{
    m_itemModel->endRemoveRows();
}
bool QGenericItemModelImplBase::beginMoveRows(const QModelIndex &sourceParent, int sourceFirst,
                                              int sourceLast,
                                              const QModelIndex &destParent, int destRow)
{
    return m_itemModel->beginMoveRows(sourceParent, sourceFirst, sourceLast, destParent, destRow);
}
void QGenericItemModelImplBase::endMoveRows()
{
    m_itemModel->endMoveRows();
}

template <typename Structure, typename Range,
          typename Protocol = QGenericItemModelDetails::table_protocol_t<Range>>
class QGenericItemModelImpl : public QGenericItemModelImplBase
{
    Q_DISABLE_COPY_MOVE(QGenericItemModelImpl)
public:
    using range_type = QGenericItemModelDetails::wrapped_t<Range>;
    using row_reference = decltype(*QGenericItemModelDetails::begin(std::declval<range_type&>()));
    using const_row_reference = decltype(*QGenericItemModelDetails::cbegin(std::declval<range_type&>()));
    using row_type = std::remove_reference_t<row_reference>;
    using protocol_type = QGenericItemModelDetails::wrapped_t<Protocol>;

protected:
    using Self = QGenericItemModelImpl<Structure, Range, Protocol>;
    Structure& that() { return static_cast<Structure &>(*this); }
    const Structure& that() const { return static_cast<const Structure &>(*this); }

    template <typename C>
    static constexpr int size(const C &c)
    {
        if (!QGenericItemModelDetails::isValid(c))
            return 0;

        if constexpr (QGenericItemModelDetails::test_size<C>()) {
            return int(std::size(c));
        } else {
#if defined(__cpp_lib_ranges)
            return int(std::ranges::distance(QGenericItemModelDetails::begin(c),
                                             QGenericItemModelDetails::end(c)));
#else
            return int(std::distance(QGenericItemModelDetails::begin(c),
                                     QGenericItemModelDetails::end(c)));
#endif
        }
    }

    friend class tst_QGenericItemModel;
    using range_features = QGenericItemModelDetails::range_traits<range_type>;
    using wrapped_row_type = QGenericItemModelDetails::wrapped_t<row_type>;
    using row_features = QGenericItemModelDetails::range_traits<wrapped_row_type>;
    using row_traits = QGenericItemModelDetails::row_traits<std::remove_cv_t<wrapped_row_type>>;
    using protocol_traits = QGenericItemModelDetails::protocol_traits<Range, protocol_type>;

    static constexpr bool isMutable()
    {
        return range_features::is_mutable && row_features::is_mutable
            && std::is_reference_v<row_reference>
            && Structure::is_mutable_impl;
    }

    static constexpr int static_row_count = QGenericItemModelDetails::static_size_v<range_type>;
    static constexpr bool rows_are_raw_pointers = std::is_pointer_v<row_type>;
    static constexpr bool rows_are_owning_or_raw_pointers =
            QGenericItemModelDetails::is_owning_or_raw_pointer<row_type>();
    static constexpr int static_column_count = QGenericItemModelDetails::static_size_v<row_type>;
    static constexpr bool one_dimensional_range = static_column_count == 0;

    static constexpr bool dynamicRows() { return isMutable() && static_row_count < 0; }
    static constexpr bool dynamicColumns() { return static_column_count < 0; }

    // A row might be a value (or range of values), or a pointer.
    // row_ptr is always a pointer, and const_row_ptr is a pointer to const.
    using row_ptr = wrapped_row_type *;
    using const_row_ptr = const wrapped_row_type *;

    template <typename T>
    static constexpr bool has_metaobject =
        (QtPrivate::QMetaTypeForType<std::remove_pointer_t<T>>::flags() & QMetaType::IsGadget)
     || (QtPrivate::QMetaTypeForType<T>::flags() & QMetaType::PointerToQObject);

    using ModelData = QGenericItemModelDetails::ModelData<std::conditional_t<
                                                    std::is_pointer_v<Range>,
                                                    Range, std::remove_reference_t<Range>>
                                                >;

    // A iterator type to use as the input iterator with the
    // range_type::insert(pos, start, end) overload if available (it is in
    // std::vector, but not in QList). Generates a prvalue when dereferenced,
    // which then gets moved into the newly constructed row, which allows us to
    // implement insertRows() for move-only row types.
    struct EmptyRowGenerator
    {
        using value_type = row_type;
        using reference = value_type;
        using pointer = value_type *;
        using iterator_category = std::input_iterator_tag;
        using difference_type = int;

        value_type operator*() { return impl->makeEmptyRow(*parent); }
        EmptyRowGenerator &operator++() { ++n; return *this; }
        friend bool operator==(const EmptyRowGenerator &lhs, const EmptyRowGenerator &rhs) noexcept
        { return lhs.n == rhs.n; }
        friend bool operator!=(const EmptyRowGenerator &lhs, const EmptyRowGenerator &rhs) noexcept
        { return !(lhs == rhs); }

        difference_type n = 0;
        Structure *impl = nullptr;
        const QModelIndex* parent = nullptr;
    };

    // If we have a move-only row_type and can add/remove rows, then the range
    // must have an insert-from-range overload.
    static_assert(static_row_count || range_features::has_insert_range
                                   || std::is_copy_constructible_v<row_type>,
                  "The range holding a move-only row-type must support insert(pos, start, end)");

public:
    explicit QGenericItemModelImpl(Range &&model, Protocol&& protocol, QGenericItemModel *itemModel)
        : QGenericItemModelImplBase(itemModel, static_cast<const Self*>(nullptr))
        , m_data{std::forward<Range>(model)}
        , m_protocol(std::forward<Protocol>(protocol))
    {
    }

    // static interface, called by QGenericItemModelImplBase
    static void callConst(ConstOp op, const QGenericItemModelImplBase *that, void *r, const void *args)
    {
        switch (op) {
        case Index: makeCall(that, &Self::index, r, args);
            break;
        case Parent: makeCall(that, &Structure::parent, r, args);
            break;
        case Sibling: makeCall(that, &Self::sibling, r, args);
            break;
        case RowCount: makeCall(that, &Structure::rowCount, r, args);
            break;
        case ColumnCount: makeCall(that, &Structure::columnCount, r, args);
            break;
        case Flags: makeCall(that, &Self::flags, r, args);
            break;
        case HeaderData: makeCall(that, &Self::headerData, r, args);
            break;
        case Data: makeCall(that, &Self::data, r, args);
            break;
        case ItemData: makeCall(that, &Self::itemData, r, args);
            break;
        }
    }

    static void call(Op op, QGenericItemModelImplBase *that, void *r, const void *args)
    {
        switch (op) {
        case Destroy: delete static_cast<Structure *>(that);
            break;
        case SetData: makeCall(that, &Self::setData, r, args);
            break;
        case SetItemData: makeCall(that, &Self::setItemData, r, args);
            break;
        case ClearItemData: makeCall(that, &Self::clearItemData, r, args);
            break;
        case InsertColumns: makeCall(that, &Self::insertColumns, r, args);
            break;
        case RemoveColumns: makeCall(that, &Self::removeColumns, r, args);
            break;
        case MoveColumns: makeCall(that, &Self::moveColumns, r, args);
            break;
        case InsertRows: makeCall(that, &Self::insertRows, r, args);
            break;
        case RemoveRows: makeCall(that, &Self::removeRows, r, args);
            break;
        case MoveRows: makeCall(that, &Self::moveRows, r, args);
            break;
        }
    }

    // actual implementations
    QModelIndex index(int row, int column, const QModelIndex &parent) const
    {
        if (row < 0 || column < 0 || column >= that().columnCount(parent)
                                  || row >= that().rowCount(parent)) {
            return {};
        }

        return that().indexImpl(row, column, parent);
    }

    QModelIndex sibling(int row, int column, const QModelIndex &index) const
    {
        if (row == index.row() && column == index.column())
            return index;

        if (column < 0 || column >= m_itemModel->columnCount())
            return {};

        if (row == index.row())
            return createIndex(row, column, index.constInternalPointer());

        const_row_ptr parentRow = static_cast<const_row_ptr>(index.constInternalPointer());
        const auto siblingCount = size(that().childrenOf(parentRow));
        if (row < 0 || row >= int(siblingCount))
            return {};
        return createIndex(row, column, parentRow);
    }

    Qt::ItemFlags flags(const QModelIndex &index) const
    {
        if (!index.isValid())
            return Qt::NoItemFlags;

        Qt::ItemFlags f = Structure::defaultFlags();

        if constexpr (has_metaobject<row_type>) {
            if (index.column() < row_traits::fixed_size()) {
                const QMetaObject mo = std::remove_pointer_t<row_type>::staticMetaObject;
                const QMetaProperty prop = mo.property(index.column() + mo.propertyOffset());
                if (prop.isWritable())
                    f |= Qt::ItemIsEditable;
            }
        } else if constexpr (static_column_count <= 0) {
            if constexpr (isMutable())
                f |= Qt::ItemIsEditable;
        } else if constexpr (std::is_reference_v<row_reference> && !std::is_const_v<row_reference>) {
            // we want to know if the elements in the tuple are const; they'd always be, if
            // we didn't remove the const of the range first.
            const_row_reference row = rowData(index);
            row_reference mutableRow = const_cast<row_reference>(row);
            if (QGenericItemModelDetails::isValid(mutableRow)) {
                for_element_at(mutableRow, index.column(), [&f](auto &&ref){
                    using target_type = decltype(ref);
                    if constexpr (std::is_const_v<std::remove_reference_t<target_type>>)
                        f &= ~Qt::ItemIsEditable;
                    else if constexpr (std::is_lvalue_reference_v<target_type>)
                        f |= Qt::ItemIsEditable;
                });
            } else {
                // If there's no usable value stored in the row, then we can't
                // do anything with this item.
                f &= ~Qt::ItemIsEditable;
            }
        }
        return f;
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role) const
    {
        QVariant result;
        if (role != Qt::DisplayRole || orientation != Qt::Horizontal
         || section < 0 || section >= that().columnCount({})) {
            return m_itemModel->QAbstractItemModel::headerData(section, orientation, role);
        }

        if constexpr (has_metaobject<row_type>) {
            using meta_type = std::remove_pointer_t<row_type>;
            if (row_traits::fixed_size() == 1) {
                const QMetaType metaType = QMetaType::fromType<meta_type>();
                result = QString::fromUtf8(metaType.name());
            } else if (section <= row_traits::fixed_size()) {
                const QMetaProperty prop = meta_type::staticMetaObject.property(
                                        section + meta_type::staticMetaObject.propertyOffset());
                result = QString::fromUtf8(prop.name());
            }
        } else if constexpr (static_column_count >= 1) {
            const QMetaType metaType = meta_type_at<row_type>(section);
            if (metaType.isValid())
                result = QString::fromUtf8(metaType.name());
        }
        if (!result.isValid())
            result = m_itemModel->QAbstractItemModel::headerData(section, orientation, role);
        return result;
    }

    QVariant data(const QModelIndex &index, int role) const
    {
        QVariant result;
        const auto readData = [this, column = index.column(), &result, role](const auto &value) {
            Q_UNUSED(this);
            using value_type = q20::remove_cvref_t<decltype(value)>;
            using multi_role = QGenericItemModelDetails::is_multi_role<value_type>;
            if constexpr (has_metaobject<value_type>) {
                if (row_traits::fixed_size() <= 1) {
                    result = readRole(role, value);
                } else if (column <= row_traits::fixed_size()
                        && (role == Qt::DisplayRole || role == Qt::EditRole)) {
                    result = readProperty(column, value);
                }
            } else if constexpr (multi_role::value) {
                const auto it = [this, &value, role]{
                    Q_UNUSED(this);
                    if constexpr (multi_role::int_key)
                        return std::as_const(value).find(Qt::ItemDataRole(role));
                    else
                        return std::as_const(value).find(roleNames().value(role));
                }();
                if (it != value.cend()) {
                    result = QGenericItemModelDetails::value(it);
                }
            } else if (role == Qt::DisplayRole || role == Qt::EditRole) {
                result = read(value);
            }
        };

        if (index.isValid())
            readAt(index, readData);

        return result;
    }

    QMap<int, QVariant> itemData(const QModelIndex &index) const
    {
        QMap<int, QVariant> result;
        bool tried = false;
        const auto readItemData = [this, &result, &tried](auto &&value){
            Q_UNUSED(this);
            using value_type = q20::remove_cvref_t<decltype(value)>;
            using multi_role = QGenericItemModelDetails::is_multi_role<value_type>;
            if constexpr (multi_role()) {
                tried = true;
                if constexpr (std::is_convertible_v<value_type, decltype(result)>) {
                    result = value;
                } else {
                    for (auto it = std::cbegin(value); it != std::cend(value); ++it) {
                        int role = [this, key = QGenericItemModelDetails::key(it)]() {
                            Q_UNUSED(this);
                            if constexpr (multi_role::int_key)
                                return int(key);
                            else
                                return roleNames().key(key.toUtf8(), -1);
                        }();

                        if (role != -1)
                            result.insert(role, QGenericItemModelDetails::value(it));
                    }
                }
            } else if constexpr (has_metaobject<value_type>) {
                if (row_traits::fixed_size() <= 1) {
                    tried = true;
                    using meta_type = std::remove_pointer_t<value_type>;
                    const QMetaObject &mo = meta_type::staticMetaObject;
                    for (auto &&[role, roleName] : roleNames().asKeyValueRange()) {
                        QVariant data;
                        if constexpr (std::is_base_of_v<QObject, meta_type>) {
                            if (value)
                                data = value->property(roleName);
                        } else {
                            const int pi = mo.indexOfProperty(roleName.constData());
                            if (pi >= 0) {
                                const QMetaProperty prop = mo.property(pi);
                                if (prop.isValid())
                                    data = prop.readOnGadget(QGenericItemModelDetails::pointerTo(value));
                            }
                        }
                        if (data.isValid())
                            result[role] = std::move(data);
                    }
                }
            }
        };

        if (index.isValid()) {
            readAt(index, readItemData);

            if (!tried) // no multi-role item found
                result = m_itemModel->QAbstractItemModel::itemData(index);
        }
        return result;
    }

    bool setData(const QModelIndex &index, const QVariant &data, int role)
    {
        if (!index.isValid())
            return false;

        bool success = false;
        if constexpr (isMutable()) {
            auto emitDataChanged = qScopeGuard([&success, this, &index, &role]{
                if (success) {
                    Q_EMIT dataChanged(index, index, role == Qt::EditRole
                                                     ? QList<int>{} : QList{role});
                }
            });

            const auto writeData = [this, column = index.column(), &data, role](auto &&target) -> bool {
                using value_type = q20::remove_cvref_t<decltype(target)>;
                using multi_role = QGenericItemModelDetails::is_multi_role<value_type>;
                if constexpr (has_metaobject<value_type>) {
                    if (QMetaType::fromType<value_type>() == data.metaType()) {
                        if constexpr (std::is_copy_assignable_v<value_type>) {
                            target = data.value<value_type>();
                            return true;
                        } else {
                            qCritical("Cannot assign %s", QMetaType::fromType<value_type>().name());
                            return false;
                        }
                    } else if (row_traits::fixed_size() <= 1) {
                        return writeRole(role, target, data);
                    } else if (column <= row_traits::fixed_size()
                            && (role == Qt::DisplayRole || role == Qt::EditRole)) {
                        return writeProperty(column, target, data);
                    }
                } else if constexpr (multi_role::value) {
                    Qt::ItemDataRole roleToSet = Qt::ItemDataRole(role);
                    // If there is an entry for EditRole, overwrite that; otherwise,
                    // set the entry for DisplayRole.
                    if (role == Qt::EditRole) {
                        if constexpr (multi_role::int_key) {
                            if (target.find(roleToSet) == target.end())
                                roleToSet = Qt::DisplayRole;
                        } else {
                            if (target.find(roleNames().value(roleToSet)) == target.end())
                                roleToSet = Qt::DisplayRole;
                        }
                    }
                    if constexpr (multi_role::int_key)
                        return write(target[roleToSet], data);
                    else
                        return write(target[roleNames().value(roleToSet)], data);
                } else if (role == Qt::DisplayRole || role == Qt::EditRole) {
                    return write(target, data);
                }
                return false;
            };

            success = writeAt(index, writeData);
        }
        return success;
    }

    bool setItemData(const QModelIndex &index, const QMap<int, QVariant> &data)
    {
        if (!index.isValid() || data.isEmpty())
            return false;

        bool success = false;
        if constexpr (isMutable()) {
            auto emitDataChanged = qScopeGuard([&success, this, &index, &data]{
                if (success)
                    Q_EMIT dataChanged(index, index, data.keys());
            });

            bool tried = false;
            auto writeItemData = [this, &tried, &data](auto &target) -> bool {
                Q_UNUSED(this);
                using value_type = q20::remove_cvref_t<decltype(target)>;
                using multi_role = QGenericItemModelDetails::is_multi_role<value_type>;
                if constexpr (multi_role()) {
                    using key_type = typename value_type::key_type;
                    tried = true;
                    const auto roleName = [map = roleNames()](int role) { return map.value(role); };

                    // transactional: only update target if all values from data
                    // can be stored. Storing never fails with int-keys.
                    if constexpr (!multi_role::int_key)
                    {
                        auto invalid = std::find_if(data.keyBegin(), data.keyEnd(),
                            [&roleName](int role) { return roleName(role).isEmpty(); }
                        );

                        if (invalid != data.keyEnd()) {
                            qWarning("No role name set for %d", *invalid);
                            return false;
                        }
                    }

                    for (auto &&[role, value] : data.asKeyValueRange()) {
                        if constexpr (multi_role::int_key)
                            target[static_cast<key_type>(role)] = value;
                        else
                            target[QString::fromUtf8(roleName(role))] = value;
                    }
                    return true;
                } else if constexpr (has_metaobject<value_type>) {
                    if (row_traits::fixed_size() <= 1) {
                        tried = true;
                        using meta_type = std::remove_pointer_t<value_type>;
                        const QMetaObject &mo = meta_type::staticMetaObject;
                        // transactional: if possible, modify a copy and only
                        // update target if all values from data could be stored.
                        auto targetCopy = [](auto &&origin) {
                            if constexpr (std::is_base_of_v<QObject, meta_type>)
                                return origin; // can't copy, no transaction support
                            else if constexpr (std::is_pointer_v<decltype(target)>)
                                return *origin;
                            else if constexpr (std::is_copy_assignable_v<value_type>)
                                return origin;
                            else // can't copy - targetCopy is now a pointer
                                return &origin;
                        }(target);
                        for (auto &&[role, value] : data.asKeyValueRange()) {
                            const QByteArray roleName = roleNames().value(role);
                            bool written = false;
                            if constexpr (std::is_base_of_v<QObject, meta_type>) {
                                if (targetCopy)
                                    written = targetCopy->setProperty(roleName, value);
                            } else {
                                const int pi = mo.indexOfProperty(roleName.constData());
                                if (pi >= 0) {
                                    const QMetaProperty prop = mo.property(pi);
                                    if (prop.isValid())
                                        written = prop.writeOnGadget(QGenericItemModelDetails::pointerTo(targetCopy), value);
                                }
                            }
                            if (!written) {
                                qWarning("Failed to write value for %s", roleName.data());
                                return false;
                            }
                        }
                        if constexpr (std::is_base_of_v<QObject, meta_type>)
                            target = targetCopy; // nothing actually copied
                        else if constexpr (std::is_pointer_v<decltype(target)>)
                            qSwap(*target, targetCopy);
                        else if constexpr (std::is_pointer_v<decltype(targetCopy)>)
                            ; // couldn't copy
                        else
                            qSwap(target, targetCopy);
                        return true;
                    }
                }
                return false;
            };

            success = writeAt(index, writeItemData);

            if (!tried) {
                // setItemData will emit the dataChanged signal
                Q_ASSERT(!success);
                emitDataChanged.dismiss();
                success = m_itemModel->QAbstractItemModel::setItemData(index, data);
            }
        }
        return success;
    }

    bool clearItemData(const QModelIndex &index)
    {
        if (!index.isValid())
            return false;

        bool success = false;
        if constexpr (isMutable()) {
            auto emitDataChanged = qScopeGuard([&success, this, &index]{
                if (success)
                    Q_EMIT dataChanged(index, index, {});
            });

            auto clearData = [column = index.column()](auto &&target) {
                if constexpr (has_metaobject<row_type>) {
                    if (row_traits::fixed_size() <= 1) {
                        // multi-role object/gadget: reset all properties
                        return resetProperty(-1, target);
                    } else if (column <= row_traits::fixed_size()) {
                        return resetProperty(column, target);
                    }
                } else { // normal structs, values, associative containers
                    target = {};
                    return true;
                }
                return false;
            };

            success = writeAt(index, clearData);
        }
        return success;
    }

    bool insertColumns(int column, int count, const QModelIndex &parent)
    {
        if constexpr (dynamicColumns() && isMutable() && row_features::has_insert) {
            if (count == 0)
                return false;
            range_type * const children = childRange(parent);
            if (!children)
                return false;

            beginInsertColumns(parent, column, column + count - 1);
            for (auto &child : *children) {
                auto it = QGenericItemModelDetails::pos(child, column);
                QGenericItemModelDetails::refTo(child).insert(it, count, {});
            }
            endInsertColumns();
            return true;
        }
        return false;
    }

    bool removeColumns(int column, int count, const QModelIndex &parent)
    {
        if constexpr (dynamicColumns() && isMutable() && row_features::has_erase) {
            if (column < 0 || column + count > that().columnCount(parent))
                return false;

            range_type * const children = childRange(parent);
            if (!children)
                return false;

            beginRemoveColumns(parent, column, column + count - 1);
            for (auto &child : *children) {
                const auto start = QGenericItemModelDetails::pos(child, column);
                QGenericItemModelDetails::refTo(child).erase(start, std::next(start, count));
            }
            endRemoveColumns();
            return true;
        }
        return false;
    }

    bool moveColumns(const QModelIndex &sourceParent, int sourceColumn, int count,
                     const QModelIndex &destParent, int destColumn)
    {
        // we only support moving columns within the same parent
        if (sourceParent != destParent)
            return false;
        if constexpr (isMutable()) {
            if (!Structure::canMoveColumns(sourceParent, destParent))
                return false;

            if constexpr (dynamicColumns()) {
                // we only support ranges as columns, as other types might
                // not have the same data type across all columns
                range_type * const children = childRange(sourceParent);
                if (!children)
                    return false;

                if (!beginMoveColumns(sourceParent, sourceColumn, sourceColumn + count - 1,
                                      destParent, destColumn)) {
                    return false;
                }

                for (auto &child : *children) {
                    const auto first = QGenericItemModelDetails::pos(child, sourceColumn);
                    const auto middle = std::next(first, count);
                    const auto last = QGenericItemModelDetails::pos(child, destColumn);

                    if (sourceColumn < destColumn) // moving right
                        std::rotate(first, middle, last);
                    else // moving left
                        std::rotate(last, first, middle);
                }

                endMoveColumns();
                return true;
            }
        }
        return false;
    }

    bool insertRows(int row, int count, const QModelIndex &parent)
    {
        if constexpr (canInsertRows()) {
            range_type *children = childRange(parent);
            if (!children)
                return false;

            EmptyRowGenerator generator{0, &that(), &parent};

            beginInsertRows(parent, row, row + count - 1);

            const auto pos = QGenericItemModelDetails::pos(children, row);
            if constexpr (range_features::has_insert_range) {
                children->insert(pos, generator, EmptyRowGenerator{count});
            } else if constexpr (rows_are_owning_or_raw_pointers) {
                auto start = children->insert(pos, count, row_type{});
                std::copy(generator, EmptyRowGenerator{count}, start);
            } else {
                children->insert(pos, count, *generator);
            }

            // fix the parent in all children of the modified row, as the
            // references back to the parent might have become invalid.
            that().resetParentInChildren(children);

            endInsertRows();
            return true;
        } else {
            return false;
        }
    }

    bool removeRows(int row, int count, const QModelIndex &parent = {})
    {
        if constexpr (Structure::canRemoveRows()) {
            const int prevRowCount = that().rowCount(parent);
            if (row < 0 || row + count > prevRowCount)
                return false;

            range_type *children = childRange(parent);
            if (!children)
                return false;

            beginRemoveRows(parent, row, row + count - 1);
            [[maybe_unused]] bool callEndRemoveColumns = false;
            if constexpr (dynamicColumns()) {
                // if we remove the last row in a dynamic model, then we no longer
                // know how many columns we should have, so they will be reported as 0.
                if (prevRowCount == count) {
                    if (const int columns = that().columnCount(parent)) {
                        callEndRemoveColumns = true;
                        beginRemoveColumns(parent, 0, columns - 1);
                    }
                }
            }
            { // erase invalidates iterators
                const auto begin = QGenericItemModelDetails::pos(children, row);
                const auto end = std::next(begin, count);
                if constexpr (rows_are_raw_pointers)
                    that().deleteRemovedRows(begin, end);
                children->erase(begin, end);
            }
            // fix the parent in all children of the modified row, as the
            // references back to the parent might have become invalid.
            that().resetParentInChildren(children);

            if constexpr (dynamicColumns()) {
                if (callEndRemoveColumns) {
                    Q_ASSERT(that().columnCount(parent) == 0);
                    endRemoveColumns();
                }
            }
            endRemoveRows();
            return true;
        } else {
            return false;
        }
    }

    bool moveRows(const QModelIndex &sourceParent, int sourceRow, int count,
                  const QModelIndex &destParent, int destRow)
    {
        if constexpr (isMutable()) {
            if (!Structure::canMoveRows(sourceParent, destParent))
                return false;

            if (sourceParent != destParent) {
                return that().moveRowsAcross(sourceParent, sourceRow, count,
                                             destParent, destRow);
            }

            if (sourceRow == destRow || sourceRow == destRow - 1 || count <= 0
             || sourceRow < 0 || sourceRow + count - 1 >= m_itemModel->rowCount(sourceParent)
             || destRow < 0 || destRow > m_itemModel->rowCount(destParent)) {
                return false;
            }

            range_type *source = childRange(sourceParent);
            // moving within the same range
            if (!beginMoveRows(sourceParent, sourceRow, sourceRow + count - 1, destParent, destRow))
                return false;

            const auto first = QGenericItemModelDetails::pos(source, sourceRow);
            const auto middle = std::next(first, count);
            const auto last = QGenericItemModelDetails::pos(source, destRow);

            if (sourceRow < destRow) // moving down
                std::rotate(first, middle, last);
            else // moving up
                std::rotate(last, first, middle);

            that().resetParentInChildren(source);

            endMoveRows();
            return true;
        } else {
            return false;
        }
    }

protected:
    ~QGenericItemModelImpl()
    {
        if constexpr (rows_are_raw_pointers && !std::is_pointer_v<Range>
                   && !QGenericItemModelDetails::is_any_of<Range, std::reference_wrapper,
                                                                  std::shared_ptr,
                                                                  QSharedPointer,
                                                                  std::unique_ptr>()) {
            // If data with rows as pointers was moved in, then we own it and
            // have to delete those rows.
            using ref = decltype(std::forward<Range>(std::declval<range_type>()));
            if constexpr (std::is_rvalue_reference_v<ref>)
                that().destroyOwnedModel(*m_data.model());
        }
    }

    static constexpr bool canInsertRows()
    {
        if constexpr (dynamicColumns() && !row_features::has_resize) {
            // If we operate on dynamic columns and cannot resize a newly
            // constructed row, then we cannot insert.
            return false;
        } else if constexpr (!one_dimensional_range && !rows_are_raw_pointers
                          && !protocol_traits::initializes_rows) {
            // We also cannot insert if the row we have to create is not
            // default-initialized by the protocol
            return false;
        } else if constexpr (!range_features::has_insert_range
                          && !std::is_copy_constructible_v<row_type>) {
            // And if the row is a move-only type, then the range needs to be
            // backed by a container that can move-insert default-constructed
            // row elements.
            return false;
        } else {
            return Structure::canInsertRows();
        }
    }

    template <typename F>
    bool writeAt(const QModelIndex &index, F&& writer)
    {
        bool result = false;
        row_reference row = rowData(index);

        if constexpr (one_dimensional_range) {
            result = writer(row);
        } else if (QGenericItemModelDetails::isValid(row)) {
            if constexpr (dynamicColumns()) {
                result = writer(*QGenericItemModelDetails::pos(row, index.column()));
            } else {
                for_element_at(row, index.column(), [&writer, &result](auto &&target) {
                    using target_type = decltype(target);
                    // we can only assign to an lvalue reference
                    if constexpr (std::is_lvalue_reference_v<target_type>
                              && !std::is_const_v<std::remove_reference_t<target_type>>) {
                        result = writer(std::forward<target_type>(target));
                    }
                });
            }
        }

        return result;
    }

    template <typename F>
    void readAt(const QModelIndex &index, F&& reader) const {
        const_row_reference row = rowData(index);
        if constexpr (one_dimensional_range) {
            return reader(row);
        } else if (QGenericItemModelDetails::isValid(row)) {
            if constexpr (dynamicColumns())
                reader(*QGenericItemModelDetails::cpos(row, index.column()));
            else
                for_element_at(row, index.column(), std::forward<F>(reader));
        }
    }

    template <typename Value>
    static QVariant read(const Value &value)
    {
        if constexpr (std::is_constructible_v<QVariant, Value>)
            return QVariant(value);
        else
            return QVariant::fromValue(value);
    }
    template <typename Value>
    static QVariant read(Value *value)
    {
        if (value) {
            if constexpr (std::is_constructible_v<QVariant, Value *>)
                return QVariant(value);
            else
                return read(*value);
        }
        return {};
    }

    template <typename Target>
    static bool write(Target &target, const QVariant &value)
    {
        using Type = std::remove_reference_t<Target>;
        if constexpr (std::is_constructible_v<Target, QVariant>) {
            target = value;
            return true;
        } else if (value.canConvert<Type>()) {
            target = value.value<Type>();
            return true;
        }
        return false;
    }
    template <typename Target>
    static bool write(Target *target, const QVariant &value)
    {
        if (target)
            return write(*target, value);
        return false;
    }

    template <typename ItemType>
    QMetaProperty roleProperty(int role) const
    {
        const QMetaObject *mo = &ItemType::staticMetaObject;
        const QByteArray roleName = roleNames().value(role);
        if (const int index = mo->indexOfProperty(roleName.data()); index >= 0)
            return mo->property(index);
        return {};
    }

    template <typename ItemType>
    QVariant readRole(int role, ItemType *gadget) const
    {
        using item_type = std::remove_pointer_t<ItemType>;
        QVariant result;
        QMetaProperty prop = roleProperty<item_type>(role);
        if (!prop.isValid() && role == Qt::EditRole)
            prop = roleProperty<item_type>(Qt::DisplayRole);

        if (prop.isValid())
            result = readProperty(prop, gadget);
        return result;
    }

    template <typename ItemType>
    QVariant readRole(int role, const ItemType &gadget) const
    {
        return readRole(role, &gadget);
    }

    template <typename ItemType>
    static QVariant readProperty(const QMetaProperty &prop, ItemType *gadget)
    {
        if constexpr (std::is_base_of_v<QObject, ItemType>)
            return prop.read(gadget);
        else
            return prop.readOnGadget(gadget);
    }
    template <typename ItemType>
    static QVariant readProperty(int property, ItemType *gadget)
    {
        using item_type = std::remove_pointer_t<ItemType>;
        const QMetaObject &mo = item_type::staticMetaObject;
        const QMetaProperty prop = mo.property(property + mo.propertyOffset());
        return readProperty(prop, gadget);
    }

    template <typename ItemType>
    static QVariant readProperty(int property, const ItemType &gadget)
    {
        return readProperty(property, &gadget);
    }

    template <typename ItemType>
    bool writeRole(int role, ItemType *gadget, const QVariant &data)
    {
        using item_type = std::remove_pointer_t<ItemType>;
        auto prop = roleProperty<item_type>(role);
        if (!prop.isValid() && role == Qt::EditRole)
            prop = roleProperty<item_type>(Qt::DisplayRole);

        return writeProperty(prop, gadget, data);
    }

    template <typename ItemType>
    bool writeRole(int role, ItemType &&gadget, const QVariant &data)
    {
        return writeRole(role, &gadget, data);
    }

    template <typename ItemType>
    static bool writeProperty(const QMetaProperty &prop, ItemType *gadget, const QVariant &data)
    {
        if constexpr (std::is_base_of_v<QObject, ItemType>)
            return prop.write(gadget, data);
        else
            return prop.writeOnGadget(gadget, data);
    }
    template <typename ItemType>
    static bool writeProperty(int property, ItemType *gadget, const QVariant &data)
    {
        using item_type = std::remove_pointer_t<ItemType>;
        const QMetaObject &mo = item_type::staticMetaObject;
        return writeProperty(mo.property(property + mo.propertyOffset()), gadget, data);
    }

    template <typename ItemType>
    static bool writeProperty(int property, ItemType &&gadget, const QVariant &data)
    {
        return writeProperty(property, &gadget, data);
    }

    template <typename ItemType>
    static bool resetProperty(int property, ItemType *object)
    {
        using item_type = std::remove_pointer_t<ItemType>;
        const QMetaObject &mo = item_type::staticMetaObject;
        bool success = true;
        if (property == -1) {
            // reset all properties
            if constexpr (std::is_base_of_v<QObject, item_type>) {
                for (int p = mo.propertyOffset(); p < mo.propertyCount(); ++p)
                    success = writeProperty(mo.property(p), object, {}) && success;
            } else { // reset a gadget by assigning a default-constructed
                *object = {};
            }
        } else {
            success = writeProperty(mo.property(property + mo.propertyOffset()), object, {});
        }
        return success;
    }

    template <typename ItemType>
    static bool resetProperty(int property, ItemType &&object)
    {
        return resetProperty(property, &object);
    }

    // helpers
    const_row_reference rowData(const QModelIndex &index) const
    {
        Q_ASSERT(index.isValid());
        return that().rowDataImpl(index);
    }

    row_reference rowData(const QModelIndex &index)
    {
        Q_ASSERT(index.isValid());
        return that().rowDataImpl(index);
    }

    const range_type *childRange(const QModelIndex &index) const
    {
        if (!index.isValid())
            return m_data.model();
        if (index.column()) // only items at column 0 can have children
            return nullptr;
        return that().childRangeImpl(index);
    }

    range_type *childRange(const QModelIndex &index)
    {
        if (!index.isValid())
            return m_data.model();
        if (index.column()) // only items at column 0 can have children
            return nullptr;
        return that().childRangeImpl(index);
    }


    const protocol_type& protocol() const { return QGenericItemModelDetails::refTo(m_protocol); }
    protocol_type& protocol() { return QGenericItemModelDetails::refTo(m_protocol); }

    ModelData m_data;
    Protocol m_protocol;
};

// Implementations that depends on the model structure (flat vs tree) that will
// be specialized based on a protocol type. The main template implements tree
// support through a protocol type.
template <typename Range, typename Protocol>
class QGenericTreeItemModelImpl
    : public QGenericItemModelImpl<QGenericTreeItemModelImpl<Range, Protocol>, Range, Protocol>
{
    using Base = QGenericItemModelImpl<QGenericTreeItemModelImpl<Range, Protocol>, Range, Protocol>;
    friend class QGenericItemModelImpl<QGenericTreeItemModelImpl<Range, Protocol>, Range, Protocol>;

    using range_type = typename Base::range_type;
    using range_features = typename Base::range_features;
    using row_type = typename Base::row_type;
    using row_ptr = typename Base::row_ptr;
    using const_row_ptr = typename Base::const_row_ptr;

    using tree_traits = typename Base::protocol_traits;
    static constexpr bool is_mutable_impl = tree_traits::has_mutable_childRows;

    static constexpr bool rows_are_any_refs_or_pointers = Base::rows_are_raw_pointers ||
                                 QGenericItemModelDetails::is_smart_ptr<row_type>() ||
                                 QGenericItemModelDetails::is_any_of<row_type, std::reference_wrapper>();
    static_assert(!Base::dynamicColumns(), "A tree must have a static number of columns!");

public:
    QGenericTreeItemModelImpl(Range &&model, Protocol &&p, QGenericItemModel *itemModel)
        : Base(std::forward<Range>(model), std::forward<Protocol>(p), itemModel)
    {};

protected:
    QModelIndex indexImpl(int row, int column, const QModelIndex &parent) const
    {
        if (!parent.isValid())
            return this->createIndex(row, column);
        // only items at column 0 can have children
        if (parent.column())
            return QModelIndex();

        const_row_ptr grandParent = static_cast<const_row_ptr>(parent.constInternalPointer());
        const auto &parentSiblings = childrenOf(grandParent);
        const auto it = QGenericItemModelDetails::cpos(parentSiblings, parent.row());
        return this->createIndex(row, column, QGenericItemModelDetails::pointerTo(*it));
    }

    QModelIndex parent(const QModelIndex &child) const
    {
        if (!child.isValid())
            return child;

        // no pointer to parent row - no parent
        const_row_ptr parentRow = static_cast<const_row_ptr>(child.constInternalPointer());
        if (!parentRow)
            return {};

        // get the siblings of the parent via the grand parent
        decltype(auto) grandParent = this->protocol().parentRow(QGenericItemModelDetails::refTo(parentRow));
        const range_type &parentSiblings = childrenOf(QGenericItemModelDetails::pointerTo(grandParent));
        // find the index of parentRow
        const auto begin = QGenericItemModelDetails::cbegin(parentSiblings);
        const auto end = QGenericItemModelDetails::cend(parentSiblings);
        const auto it = std::find_if(begin, end, [parentRow](auto &&s){
            return QGenericItemModelDetails::pointerTo(s) == parentRow;
        });
        if (it != end)
            return this->createIndex(std::distance(begin, it), 0,
                                     QGenericItemModelDetails::pointerTo(grandParent));
        return {};
    }

    int rowCount(const QModelIndex &parent) const
    {
        return Base::size(this->childRange(parent));
    }

    int columnCount(const QModelIndex &) const
    {
        // all levels of a tree have to have the same, static, column count
        return Base::static_column_count;
    }

    static constexpr Qt::ItemFlags defaultFlags()
    {
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    }

    static constexpr bool canInsertRows()
    {
        // We must not insert rows if we cannot adjust the parents of the
        // children of the following rows. We don't have to do that if the
        // range operates on pointers.
        return (rows_are_any_refs_or_pointers || tree_traits::has_setParentRow)
             && Base::dynamicRows() && range_features::has_insert;
    }

    static constexpr bool canRemoveRows()
    {
        // We must not remove rows if we cannot adjust the parents of the
        // children of the following rows. We don't have to do that if the
        // range operates on pointers.
        return (rows_are_any_refs_or_pointers || tree_traits::has_setParentRow)
             && Base::dynamicRows() && range_features::has_erase;
    }

    static constexpr bool canMoveColumns(const QModelIndex &, const QModelIndex &)
    {
        return true;
    }

    static constexpr bool canMoveRows(const QModelIndex &, const QModelIndex &)
    {
        return true;
    }

    bool moveRowsAcross(const QModelIndex &sourceParent, int sourceRow, int count,
                        const QModelIndex &destParent, int destRow)
    {
        // If rows are pointers, then reference to the parent row don't
        // change, so we can move them around freely. Otherwise we need to
        // be able to explicitly update the parent pointer.
        if constexpr (!rows_are_any_refs_or_pointers && !tree_traits::has_setParentRow) {
            return false;
        } else if constexpr (!(range_features::has_insert && range_features::has_erase)) {
            return false;
        } else if (!this->beginMoveRows(sourceParent, sourceRow, sourceRow + count - 1,
                                        destParent, destRow)) {
            return false;
        }

        range_type *source = this->childRange(sourceParent);
        range_type *destination = this->childRange(destParent);

        // If we can insert data from another range into, then
        // use that to move the old data over.
        const auto destStart = QGenericItemModelDetails::pos(destination, destRow);
        if constexpr (range_features::has_insert_range) {
            const auto sourceStart = QGenericItemModelDetails::pos(*source, sourceRow);
            const auto sourceEnd = std::next(sourceStart, count);

            destination->insert(destStart, std::move_iterator(sourceStart),
                                           std::move_iterator(sourceEnd));
        } else if constexpr (std::is_copy_constructible_v<row_type>) {
            // otherwise we have to make space first, and copy later.
            destination->insert(destStart, count, row_type{});
        }

        row_ptr parentRow = destParent.isValid()
                          ? QGenericItemModelDetails::pointerTo(this->rowData(destParent))
                          : nullptr;

        // if the source's parent was already inside the new parent row,
        // then the source row might have become invalid, so reset it.
        if (parentRow == static_cast<row_ptr>(sourceParent.internalPointer())) {
            if (sourceParent.row() < destRow) {
                source = this->childRange(sourceParent);
            } else {
                // the source parent moved down within destination
                source = this->childRange(this->createIndex(sourceParent.row() + count, 0,
                                                            sourceParent.internalPointer()));
            }
        }

        // move the data over and update the parent pointer
        {
            const auto writeStart = QGenericItemModelDetails::pos(destination, destRow);
            const auto writeEnd = std::next(writeStart, count);
            const auto sourceStart = QGenericItemModelDetails::pos(source, sourceRow);
            const auto sourceEnd = std::next(sourceStart, count);

            for (auto write = writeStart, read = sourceStart; write != writeEnd; ++write, ++read) {
                // move data over if not already done, otherwise
                // only fix the parent pointer
                if constexpr (!range_features::has_insert_range)
                    *write = std::move(*read);
                this->protocol().setParentRow(QGenericItemModelDetails::refTo(*write), parentRow);
            }
            // remove the old rows from the source parent
            source->erase(sourceStart, sourceEnd);
        }

        // Fix the parent pointers in children of both source and destination
        // ranges, as the references to the entries might have become invalid.
        // We don't have to do that if the rows are pointers, as in that case
        // the references to the entries are stable.
        resetParentInChildren(destination);
        resetParentInChildren(source);

        this->endMoveRows();
        return true;
    }

    auto makeEmptyRow(const QModelIndex &parent)
    {
        // tree traversal protocol: if we are here, then it must be possible
        // to change the parent of a row.
        static_assert(tree_traits::has_setParentRow);
        row_type empty_row = this->protocol().newRow();
        if (QGenericItemModelDetails::isValid(empty_row) && parent.isValid()) {
            this->protocol().setParentRow(QGenericItemModelDetails::refTo(empty_row),
                                        QGenericItemModelDetails::pointerTo(this->rowData(parent)));
        }
        return empty_row;
    }

    template <typename R>
    void destroyOwnedModel(R &&range)
    {
        const auto begin = QGenericItemModelDetails::begin(range);
        const auto end = QGenericItemModelDetails::end(range);
        deleteRemovedRows(begin, end);
    }

    template <typename It, typename Sentinel>
    void deleteRemovedRows(It &&begin, Sentinel &&end)
    {
        for (auto it = begin; it != end; ++it)
            this->protocol().deleteRow(*it);
    }

    void resetParentInChildren(range_type *children)
    {
        if constexpr (tree_traits::has_setParentRow && !rows_are_any_refs_or_pointers) {
            const auto begin = QGenericItemModelDetails::begin(*children);
            const auto end = QGenericItemModelDetails::end(*children);
            for (auto it = begin; it != end; ++it) {
                if (auto &maybeChildren = this->protocol().childRows(*it)) {
                    QModelIndexList fromIndexes;
                    QModelIndexList toIndexes;
                    fromIndexes.reserve(Base::size(*maybeChildren));
                    toIndexes.reserve(Base::size(*maybeChildren));
                    auto *parentRow = QGenericItemModelDetails::pointerTo(*it);

                    int row = 0;
                    for (auto &child : *maybeChildren) {
                        const_row_ptr oldParent = this->protocol().parentRow(child);
                        if (oldParent != parentRow) {
                            fromIndexes.append(this->createIndex(row, 0, oldParent));
                            toIndexes.append(this->createIndex(row, 0, parentRow));
                            this->protocol().setParentRow(child, parentRow);
                        }
                        ++row;
                    }
                    this->changePersistentIndexList(fromIndexes, toIndexes);
                    resetParentInChildren(QGenericItemModelDetails::pointerTo(*maybeChildren));
                }
            }
        }
    }

    decltype(auto) rowDataImpl(const QModelIndex &index) const
    {
        const_row_ptr parentRow = static_cast<const_row_ptr>(index.constInternalPointer());
        const range_type &siblings = childrenOf(parentRow);
        Q_ASSERT(index.row() < int(Base::size(siblings)));
        return *QGenericItemModelDetails::cpos(siblings, index.row());
    }

    decltype(auto) rowDataImpl(const QModelIndex &index)
    {
        row_ptr parentRow = static_cast<row_ptr>(index.internalPointer());
        range_type &siblings = childrenOf(parentRow);
        Q_ASSERT(index.row() < int(Base::size(siblings)));
        return *QGenericItemModelDetails::pos(siblings, index.row());
    }

    const range_type *childRangeImpl(const QModelIndex &index) const
    {
        const auto &row = this->rowData(index);
        if (!QGenericItemModelDetails::isValid(row))
            return static_cast<const range_type *>(nullptr);

        decltype(auto) children = this->protocol().childRows(QGenericItemModelDetails::refTo(row));
        return QGenericItemModelDetails::pointerTo(std::forward<decltype(children)>(children));
    }

    range_type *childRangeImpl(const QModelIndex &index)
    {
        auto &row = this->rowData(index);
        if (!QGenericItemModelDetails::isValid(row))
            return static_cast<range_type *>(nullptr);

        decltype(auto) children = this->protocol().childRows(QGenericItemModelDetails::refTo(row));
        using Children = std::remove_reference_t<decltype(children)>;

        if constexpr (QGenericItemModelDetails::is_any_of<Children, std::optional>()
                   && std::is_default_constructible<typename Children::value_type>()) {
            if (!children)
                children.emplace(range_type{});
        }

        return QGenericItemModelDetails::pointerTo(std::forward<decltype(children)>(children));
    }

    const range_type &childrenOf(const_row_ptr row) const
    {
        return row ? QGenericItemModelDetails::refTo(this->protocol().childRows(*row))
                   : *this->m_data.model();
    }

private:
    range_type &childrenOf(row_ptr row)
    {
        return row ? QGenericItemModelDetails::refTo(*this->protocol().childRows(*row))
                   : *this->m_data.model();
    }
};

// specialization for flat models without protocol
template <typename Range>
class QGenericTableItemModelImpl
    : public QGenericItemModelImpl<QGenericTableItemModelImpl<Range>, Range>
{
    using Base = QGenericItemModelImpl<QGenericTableItemModelImpl<Range>, Range>;
    friend class QGenericItemModelImpl<QGenericTableItemModelImpl<Range>, Range>;

    using range_type = typename Base::range_type;
    using range_features = typename Base::range_features;
    using row_type = typename Base::row_type;
    using const_row_ptr = typename Base::const_row_ptr;
    using row_traits = typename Base::row_traits;
    using row_features = typename Base::row_features;

    static constexpr bool is_mutable_impl = true;

public:
    explicit QGenericTableItemModelImpl(Range &&model, QGenericItemModel *itemModel)
        : Base(std::forward<Range>(model), {}, itemModel)
    {}

protected:
    QModelIndex indexImpl(int row, int column, const QModelIndex &) const
    {
        if constexpr (Base::dynamicColumns()) {
            if (column < int(Base::size(*QGenericItemModelDetails::cpos(*this->m_data.model(), row))))
                return this->createIndex(row, column);
            // if we got here, then column < columnCount(), but this row is to short
            qCritical("QGenericItemModel: Column-range at row %d is not large enough!", row);
            return {};
        } else {
            return this->createIndex(row, column);
        }
    }

    QModelIndex parent(const QModelIndex &) const
    {
        return {};
    }

    int rowCount(const QModelIndex &parent) const
    {
        if (parent.isValid())
            return 0;
        return int(Base::size(*this->m_data.model()));
    }

    int columnCount(const QModelIndex &parent) const
    {
        if (parent.isValid())
            return 0;

        // in a table, all rows have the same number of columns (as the first row)
        if constexpr (Base::dynamicColumns()) {
            return int(Base::size(*this->m_data.model()) == 0
                       ? 0
                       : Base::size(*QGenericItemModelDetails::cbegin(*this->m_data.model())));
        } else if constexpr (Base::one_dimensional_range) {
            return row_traits::fixed_size();
        } else {
            return Base::static_column_count;
        }
    }

    static constexpr Qt::ItemFlags defaultFlags()
    {
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemNeverHasChildren;
    }

    static constexpr bool canInsertRows()
    {
        return Base::dynamicRows() && range_features::has_insert;
    }

    static constexpr bool canRemoveRows()
    {
        return Base::dynamicRows() && range_features::has_erase;
    }

    static constexpr bool canMoveColumns(const QModelIndex &source, const QModelIndex &destination)
    {
        return !source.isValid() && !destination.isValid();
    }

    static constexpr bool canMoveRows(const QModelIndex &source, const QModelIndex &destination)
    {
        return !source.isValid() && !destination.isValid();
    }

    constexpr bool moveRowsAcross(const QModelIndex &, int , int,
                                  const QModelIndex &, int) noexcept
    {
        // table/flat model: can't move rows between different parents
        return false;
    }

    auto makeEmptyRow(const QModelIndex &)
    {
        row_type empty_row = this->protocol().newRow();

        // dynamically sized rows all have to have the same column count
        if constexpr (Base::dynamicColumns() && row_features::has_resize) {
            if (QGenericItemModelDetails::isValid(empty_row))
                QGenericItemModelDetails::refTo(empty_row).resize(Base::m_itemModel->columnCount());
        }

        return empty_row;
    }

    template <typename R>
    void destroyOwnedModel(R &&range)
    {
        const auto begin = QGenericItemModelDetails::begin(range);
        const auto end = QGenericItemModelDetails::end(range);
        for (auto it = begin; it !=  end; ++it)
            delete *it;
    }

    template <typename It, typename Sentinel>
    void deleteRemovedRows(It &&, Sentinel &&)
    {
        // nothing to do for tables and lists as we never create rows either
    }

    decltype(auto) rowDataImpl(const QModelIndex &index) const
    {
        Q_ASSERT(index.row() < int(Base::size(*this->m_data.model())));
        return *QGenericItemModelDetails::cpos(*this->m_data.model(), index.row());
    }

    decltype(auto) rowDataImpl(const QModelIndex &index)
    {
        Q_ASSERT(index.row() < int(Base::size(*this->m_data.model())));
        return *QGenericItemModelDetails::pos(*this->m_data.model(), index.row());
    }

    auto childRangeImpl(const QModelIndex &) const
    {
        return nullptr;
    }

    auto childRangeImpl(const QModelIndex &)
    {
        return nullptr;
    }

    const range_type &childrenOf(const_row_ptr row) const
    {
        Q_ASSERT(!row);
        return *this->m_data.model();
    }

    void resetParentInChildren(range_type *)
    {
    }
};

template <typename Range,
          QGenericItemModelDetails::if_is_table_range<Range>>
QGenericItemModel::QGenericItemModel(Range &&range, QObject *parent)
    : QAbstractItemModel(parent)
    , impl(new QGenericTableItemModelImpl<Range>(std::forward<Range>(range), this))
{}

template <typename Range,
         QGenericItemModelDetails::if_is_tree_range<Range>>
QGenericItemModel::QGenericItemModel(Range &&range, QObject *parent)
    : QGenericItemModel(std::forward<Range>(range),
                        QGenericItemModelDetails::DefaultTreeProtocol<Range>{}, parent)
{}

template <typename Range, typename Protocol,
          QGenericItemModelDetails::if_is_tree_range<Range, Protocol>>
QGenericItemModel::QGenericItemModel(Range &&range, Protocol &&protocol, QObject *parent)
    : QAbstractItemModel(parent)
   , impl(new QGenericTreeItemModelImpl<Range, Protocol>(std::forward<Range>(range),
                                                         std::forward<Protocol>(protocol), this))
{}

QT_END_NAMESPACE

namespace std {
    template <typename T>
    struct tuple_size<QT_PREPEND_NAMESPACE(QGenericItemModel)::MultiColumn<T>>
        : tuple_size<typename QT_PREPEND_NAMESPACE(QGenericItemModel)::MultiColumn<T>::type>
    {};
    template <std::size_t I, typename T>
    struct tuple_element<I, QT_PREPEND_NAMESPACE(QGenericItemModel)::MultiColumn<T>>
        : tuple_element<I, typename QT_PREPEND_NAMESPACE(QGenericItemModel)::MultiColumn<T>::type>
    {};
}

#endif // QGENERICITEMMODEL_H
