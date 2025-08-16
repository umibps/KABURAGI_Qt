static void RASTERIZE_EDGES(
	PIXEL_MANIPULATE_IMAGE  *image,
	PIXEL_MANIPULATE_EDGE	*l,
	PIXEL_MANIPULATE_EDGE	*r,
	PIXEL_MANIPULATE_FIXED		t,
	PIXEL_MANIPULATE_FIXED		b
)
{
	PIXEL_MANIPULATE_FIXED  y = t;
	uint32  *line;
	uint32 *buf = (image)->bits.bits;
	int stride = (image)->bits.row_stride;
	int width = (image)->bits.width;

	line = buf + PIXEL_MANIPULATE_FIXED_TO_INTEGER(y) * stride;

	for (;;)
	{
		PIXEL_MANIPULATE_FIXED	lx;
		PIXEL_MANIPULATE_FIXED	  rx;
		int	lxi;
		int rxi;

		lx = l->x;
		rx = r->x;
#if N_BITS == 1
		/* For the non-antialiased case, round the coordinates up, in effect
		* sampling just slightly to the left of the pixel. This is so that
		* when the sample point lies exactly on the line, we round towards
		* north-west.
		*
		* (The AA case does a similar  adjustment in RENDER_SAMPLES_X)
		*/
		lx += X_FRAC_FIRST(1) - PIXEL_MANIPULATE_FIXED_E;
		rx += X_FRAC_FIRST(1) - PIXEL_MANIPULATE_FIXED_E;
#endif
		/* clip X */
		if (lx < 0)
			lx = 0;
		if(PIXEL_MANIPULATE_FIXED_TO_INTEGER(rx) >= width)
#if N_BITS == 1
			rx = PIXEL_MANIPULATE_INTEGER_TO_FIXED(width);
#else
			/* Use the last pixel of the scanline, covered 100%.
			* We can't use the first pixel following the scanline,
			* because accessing it could result in a buffer overrun.
			*/
			rx = PIXEL_MANIPULATE_INTEGER_TO_FIXED(width) - 1;
#endif

		/* Skip empty (or backwards) sections */
		if (rx > lx)
		{

			/* Find pixel bounds for span */
			lxi = PIXEL_MANIPULATE_FIXED_TO_INTEGER(lx);
			rxi = PIXEL_MANIPULATE_FIXED_TO_INTEGER(rx);

#if N_BITS == 1
			{

#define LEFT_MASK(x)							\
		(((x) & 0x1f) ?						\
		 SCREEN_SHIFT_RIGHT (0xffffffff, (x) & 0x1f) : 0)
#define RIGHT_MASK(x)							\
		(((32 - (x)) & 0x1f) ?					\
		 SCREEN_SHIFT_LEFT (0xffffffff, (32 - (x)) & 0x1f) : 0)

#define MASK_BITS(x,w,l,n,r) {						\
			n = (w);						\
			r = RIGHT_MASK ((x) + n);				\
			l = LEFT_MASK (x);					\
			if (l) {						\
			n -= 32 - ((x) & 0x1f);				\
			if (n < 0) {					\
				n = 0;					\
				l &= r;					\
				r = 0;					\
			}						\
			}							\
			n >>= 5;						\
		}

				uint32  *a = line;
				uint32  startmask;
				uint32  endmask;
				int		nmiddle;
				int		width = rxi - lxi;
				int		x = lxi;

				a += x >> 5;
				x &= 0x1f;

				MASK_BITS(x, width, startmask, nmiddle, endmask);

				if (startmask)
				{
					WRITE(image, a, READ(image, a) | startmask);
					a++;
				}
				while(nmiddle--)
					WRITE(image, a++, 0xffffffff);
				if(endmask)
					WRITE(image, a, READ(image, a) | endmask);
			}
#else
			{
				DEFINE_ALPHA(line,lxi);
				int		lxs;
				int	 rxs;

				/* Sample coverage for edge pixels */
				lxs = RENDER_SAMPLES_X (lx, N_BITS);
				rxs = RENDER_SAMPLES_X (rx, N_BITS);

				/* Add coverage across row */
				if (lxi == rxi)
				{
					ADD_ALPHA (rxs - lxs);
				}
				else
				{
					int	xi;

					ADD_ALPHA (N_X_FRAC(N_BITS) - lxs);
					STEP_ALPHA;
					for (xi = lxi + 1; xi < rxi; xi++)
					{
						ADD_ALPHA (N_X_FRAC(N_BITS));
						STEP_ALPHA;
					}
					ADD_ALPHA (rxs);
				}
			}
#endif
		}

		if (y == b)
			break;

#if N_BITS > 1
		if (PIXEL_MANIPULATE_FIXED_FRAC(y) != Y_FRAC_LAST(N_BITS))
		{
			RENDER_EDGE_STEP_SMALL (l);
			RENDER_EDGE_STEP_SMALL (r);
			y += STEP_Y_SMALL(N_BITS);
		}
		else
#endif
		{
			RENDER_EDGE_STEP_BIG (l);
			RENDER_EDGE_STEP_BIG (r);
			y += STEP_Y_BIG(N_BITS);
			line += stride;
		}
	}
}
