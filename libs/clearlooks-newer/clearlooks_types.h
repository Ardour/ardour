#ifndef CLEARLOOKS_TYPES_H
#define CLEARLOOKS_TYPES_H

#include <ge-support.h>

typedef unsigned char boolean;
typedef unsigned char uint8;
typedef struct _ClearlooksStyleFunctions ClearlooksStyleFunctions;

typedef enum
{
	CL_STYLE_CLASSIC = 0,
	CL_STYLE_GLOSSY = 1,
	CL_STYLE_INVERTED = 2,
	CL_STYLE_GUMMY = 3,
	CL_NUM_STYLES = 4
} ClearlooksStyles;


typedef enum
{
	CL_STATE_NORMAL,
	CL_STATE_ACTIVE,
	CL_STATE_SELECTED,
	CL_STATE_INSENSITIVE
} ClearlooksStateType;

typedef enum
{
	CL_JUNCTION_NONE      = 0,
	CL_JUNCTION_BEGIN     = 1,
	CL_JUNCTION_END       = 2
} ClearlooksJunction;

typedef enum
{
	CL_STEPPER_UNKNOWN    = 0,
	CL_STEPPER_A          = 1,
	CL_STEPPER_B          = 2,
	CL_STEPPER_C          = 4,
	CL_STEPPER_D          = 8
} ClearlooksStepper;

typedef enum
{
	CL_ORDER_FIRST,
	CL_ORDER_MIDDLE,
	CL_ORDER_LAST
} ClearlooksOrder;

typedef enum
{
	CL_ORIENTATION_LEFT_TO_RIGHT,
	CL_ORIENTATION_RIGHT_TO_LEFT,
	CL_ORIENTATION_BOTTOM_TO_TOP,
	CL_ORIENTATION_TOP_TO_BOTTOM
} ClearlooksOrientation;

typedef enum
{
	CL_GAP_LEFT,
	CL_GAP_RIGHT,
	CL_GAP_TOP,
	CL_GAP_BOTTOM
} ClearlooksGapSide;

typedef enum
{
	CL_SHADOW_NONE,
	CL_SHADOW_IN,
	CL_SHADOW_OUT,
	CL_SHADOW_ETCHED_IN,
	CL_SHADOW_ETCHED_OUT
} ClearlooksShadowType;

typedef enum
{
	CL_HANDLE_TOOLBAR,
	CL_HANDLE_SPLITTER
} ClearlooksHandleType;

typedef enum
{
	CL_ARROW_NORMAL,
	CL_ARROW_COMBO
} ClearlooksArrowType;

typedef enum
{
	CL_DIRECTION_UP,
	CL_DIRECTION_DOWN,
	CL_DIRECTION_LEFT,
	CL_DIRECTION_RIGHT
} ClearlooksDirection;

typedef enum
{
	CL_PROGRESSBAR_CONTINUOUS,
	CL_PROGRESSBAR_DISCRETE
} ClearlooksProgressBarStyle;

typedef enum
{
	CL_WINDOW_EDGE_NORTH_WEST,
	CL_WINDOW_EDGE_NORTH,
	CL_WINDOW_EDGE_NORTH_EAST,
	CL_WINDOW_EDGE_WEST,
	CL_WINDOW_EDGE_EAST,
	CL_WINDOW_EDGE_SOUTH_WEST,
	CL_WINDOW_EDGE_SOUTH,
	CL_WINDOW_EDGE_SOUTH_EAST
} ClearlooksWindowEdge;

typedef struct
{
	double x;
	double y;
	double width;
	double height;
} ClearlooksRectangle;

typedef struct
{
	CairoColor fg[5];
	CairoColor bg[5];
	CairoColor base[5];
	CairoColor text[5];

	CairoColor shade[9];
	CairoColor spot[3];
} ClearlooksColors;

typedef struct
{
	boolean active;
	boolean prelight;
	boolean disabled;
	boolean focus;
	boolean is_default;
	boolean ltr;
	boolean enable_glow;

	gfloat  radius;

	ClearlooksStateType state_type;

	uint8 corners;
	uint8 xthickness;
	uint8 ythickness;

	CairoColor parentbg;

	ClearlooksStyleFunctions *style_functions;
} WidgetParameters;

typedef struct
{
	boolean lower;
	boolean horizontal;
	boolean fill_level;
} SliderParameters;

typedef struct
{
	ClearlooksOrientation orientation;
	boolean pulsing;
	float value;
} ProgressBarParameters;

typedef struct
{
	int linepos;
} OptionMenuParameters;

typedef struct
{
	ClearlooksShadowType shadow;
	ClearlooksGapSide gap_side;
	int gap_x;
	int gap_width;
	const CairoColor *border; /* maybe changes this to some other hint ... */
} FrameParameters;

typedef struct
{
	ClearlooksGapSide gap_side;
} TabParameters;

typedef struct
{
	CairoCorners    corners;
	ClearlooksShadowType shadow;
} ShadowParameters;

typedef struct
{
	boolean horizontal;
} SeparatorParameters;

typedef struct
{
	ClearlooksOrder order; /* XXX: rename to position */
	boolean         resizable;
} ListViewHeaderParameters;

typedef struct
{
	CairoColor         color;
	ClearlooksJunction junction;       /* On which sides the slider junctions */
	boolean            horizontal;
	boolean            has_color;
} ScrollBarParameters;

typedef struct
{
	ClearlooksHandleType type;
	boolean              horizontal;
} HandleParameters;

typedef struct
{
	ClearlooksStepper stepper;         /* Which stepper to draw */
} ScrollBarStepperParameters;

typedef struct
{
	ClearlooksWindowEdge edge;
} ResizeGripParameters;

typedef struct
{
	int style;
} MenuBarParameters;

typedef struct
{
	ClearlooksShadowType shadow_type;
	boolean              in_cell;
	boolean              in_menu;
} CheckboxParameters;

typedef struct
{
	ClearlooksArrowType type;
	ClearlooksDirection direction;
} ArrowParameters;

typedef struct
{
	int      style;
	boolean  topmost;
} ToolbarParameters;

struct _ClearlooksStyleFunctions
{
	void (*draw_button)           (cairo_t				*cr,
	                               const ClearlooksColors		*colors,
	                               const WidgetParameters		*widget,
	                               int x, int y, int width, int height);

	void (*draw_scale_trough)     (cairo_t				*cr,
	                               const ClearlooksColors		*colors,
	                               const WidgetParameters		*widget,
	                               const SliderParameters		*slider,
	                               int x, int y, int width, int height);

	void (*draw_progressbar_trough) (cairo_t			*cr,
	                               const ClearlooksColors		*colors,
	                               const WidgetParameters		*widget,
	                               int x, int y, int width, int height);

	void (*draw_progressbar_fill) (cairo_t				*cr,
	                               const ClearlooksColors		*colors,
	                               const WidgetParameters		*widget,
	                               const ProgressBarParameters	*progressbar,
	                               int x, int y, int width, int height, gint offset);

	void (*draw_slider_button)    (cairo_t				*cr,
	                               const ClearlooksColors		*colors,
	                               const WidgetParameters		*widget,
	                               const SliderParameters		*slider,
	                               int x, int y, int width, int height);

	void (*draw_entry)            (cairo_t				*cr,
	                               const ClearlooksColors		*colors,
	                               const WidgetParameters		*widget,
	                               int x, int y, int width, int height);

	void (*draw_spinbutton)       (cairo_t				*cr,
	                               const ClearlooksColors		*colors,
	                               const WidgetParameters		*widget,
	                               int x, int y, int width, int height);

	void (*draw_spinbutton_down)  (cairo_t				*cr,
	                               const ClearlooksColors		*colors,
	                               const WidgetParameters		*widget,
	                               int x, int y, int width, int height);

	void (*draw_optionmenu)       (cairo_t				*cr,
	                               const ClearlooksColors		*colors,
	                               const WidgetParameters		*widget,
	                               const OptionMenuParameters *optionmenu,
	                               int x, int y, int width, int height);

	void (*draw_inset)            (cairo_t				*cr,
	                                const CairoColor		*bg_color,
	                                double x, double y, double w, double h,
	                                double radius, uint8 corners);

	void (*draw_menubar)          (cairo_t				*cr,
	                               const ClearlooksColors		*colors,
	                               const WidgetParameters		*widget,
	                               const MenuBarParameters	*menubar,
	                               int x, int y, int width, int height);

	void (*draw_tab)              (cairo_t				*cr,
	                               const ClearlooksColors		*colors,
	                               const WidgetParameters		*widget,
	                               const TabParameters		   *tab,
	                               int x, int y, int width, int height);

	void (*draw_frame)            (cairo_t				*cr,
	                               const ClearlooksColors		*colors,
	                               const WidgetParameters		*widget,
	                               const FrameParameters		*frame,
	                               int x, int y, int width, int height);

	void (*draw_separator)        (cairo_t				*cr,
	                               const ClearlooksColors		*colors,
	                               const WidgetParameters		*widget,
	                               const SeparatorParameters	*separator,
	                               int x, int y, int width, int height);

	void (*draw_menu_item_separator) (cairo_t			*cr,
	                               const ClearlooksColors		*colors,
	                               const WidgetParameters		*widget,
	                               const SeparatorParameters	*separator,
	                               int x, int y, int width, int height);

	void (*draw_list_view_header) (cairo_t				*cr,
	                               const ClearlooksColors		*colors,
	                               const WidgetParameters		*widget,
	                               const ListViewHeaderParameters	*header,
	                               int x, int y, int width, int height);

	void (*draw_toolbar)          (cairo_t				*cr,
	                               const ClearlooksColors		*colors,
	                               const WidgetParameters		*widget,
	                               const ToolbarParameters          *toolbar,
	                               int x, int y, int width, int height);

	void (*draw_menuitem)         (cairo_t				*cr,
	                               const ClearlooksColors		*colors,
	                               const WidgetParameters		*widget,
	                               int x, int y, int width, int height);

	void (*draw_menubaritem)      (cairo_t				*cr,
	                               const ClearlooksColors		*colors,
	                               const WidgetParameters		*widget,
	                               int x, int y, int width, int height);

	void (*draw_selected_cell)    (cairo_t				*cr,
	                               const ClearlooksColors		*colors,
	                               const WidgetParameters		*widget,
	                               int x, int y, int width, int height);

	void (*draw_scrollbar_stepper) (cairo_t				*cr,
	                                const ClearlooksColors	*colors,
	                                const WidgetParameters	*widget,
	                                const ScrollBarParameters *scrollbar,
	                                const ScrollBarStepperParameters *stepper,
	                                int x, int y, int width, int height);

	void (*draw_scrollbar_slider) (cairo_t				*cr,
	                               const ClearlooksColors		*colors,
	                               const WidgetParameters		*widget,
	                               const ScrollBarParameters	*scrollbar,
	                               int x, int y, int width, int height);

	void (*draw_scrollbar_trough) (cairo_t				*cr,
	                               const ClearlooksColors		*colors,
	                               const WidgetParameters		*widget,
	                               const ScrollBarParameters	*scrollbar,
	                               int x, int y, int width, int height);

	void (*draw_statusbar)        (cairo_t				*cr,
	                               const ClearlooksColors		*colors,
	                               const WidgetParameters		*widget,
	                               int x, int y, int width, int height);

	void (*draw_menu_frame)       (cairo_t				*cr,
	                               const ClearlooksColors		*colors,
	                               const WidgetParameters		*widget,
	                               int x, int y, int width, int height);

	void (*draw_tooltip)          (cairo_t				*cr,
	                               const ClearlooksColors		*colors,
	                               const WidgetParameters		*widget,
	                               int x, int y, int width, int height);

	void (*draw_handle)           (cairo_t				*cr,
	                               const ClearlooksColors		*colors,
	                               const WidgetParameters		*widget,
	                               const HandleParameters		*handle,
	                               int x, int y, int width, int height);

	void (*draw_resize_grip)      (cairo_t				*cr,
	                               const ClearlooksColors		*colors,
	                               const WidgetParameters		*widget,
	                               const ResizeGripParameters	*grip,
	                               int x, int y, int width, int height);

	void (*draw_arrow)            (cairo_t				*cr,
	                               const ClearlooksColors		*colors,
	                               const WidgetParameters		*widget,
	                               const ArrowParameters		*arrow,
	                               int x, int y, int width, int height);

	void (*draw_checkbox)         (cairo_t				*cr,
	                               const ClearlooksColors		*colors,
	                               const WidgetParameters		*widget,
	                               const CheckboxParameters	*checkbox,
	                               int x, int y, int width, int height);

	void (*draw_radiobutton)      (cairo_t				*cr,
	                               const ClearlooksColors		*colors,
	                               const WidgetParameters		*widget,
	                               const CheckboxParameters	*checkbox,
	                               int x, int y, int width, int height);

	/* Style internal functions */
	/* XXX: Only used by slider_button, inline it? */
	void (*draw_shadow)           (cairo_t				*cr,
	                               const ClearlooksColors		*colors,
	                               gfloat				 radius,
	                               int width, int height);

	void (*draw_slider)           (cairo_t				*cr,
	                               const ClearlooksColors		*colors,
	                               const WidgetParameters		*widget,
	                               int x, int y, int width, int height);

	void (*draw_gripdots)         (cairo_t *cr,
	                               const ClearlooksColors *colors, int x, int y,
	                               int width, int height, int xr, int yr,
	                               float contrast);
};


#define CLEARLOOKS_RECTANGLE_SET(rect, _x, _y, _w, _h) rect.x      = _x; \
                                                       rect.y      = _y; \
                                                       rect.width  = _w; \
                                                       rect.height = _h;

#endif /* CLEARLOOKS_TYPES_H */
