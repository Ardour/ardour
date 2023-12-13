/* XEMBED messages */
typedef enum {
  XEMBED_EMBEDDED_NOTIFY        = 0,
  XEMBED_WINDOW_ACTIVATE        = 1,
  XEMBED_WINDOW_DEACTIVATE      = 2,
  XEMBED_REQUEST_FOCUS          = 3,
  XEMBED_FOCUS_IN               = 4,
  XEMBED_FOCUS_OUT              = 5,
  XEMBED_FOCUS_NEXT             = 6,
  XEMBED_FOCUS_PREV             = 7,
  XEMBED_GRAB_KEY               = 8,
  XEMBED_UNGRAB_KEY             = 9,
  XEMBED_MODALITY_ON            = 10,
  XEMBED_MODALITY_OFF           = 11,

/* Non standard messages*/
  XEMBED_GTK_GRAB_KEY           = 108, 
  XEMBED_GTK_UNGRAB_KEY         = 109
} XEmbedMessageType;

/* Details for  XEMBED_FOCUS_IN: */
#define XEMBED_FOCUS_CURRENT             0
#define XEMBED_FOCUS_FIRST               1
#define XEMBED_FOCUS_LAST                2

/* Flags for XEMBED_FOCUS_IN, XEMBED_FOCUS_NEXT, XEMBED_FOCUS_PREV */
#define XEMBED_FOCUS_WRAPAROUND         (1 << 0)

/* Flags for _XEMBED_INFO */
#define XEMBED_MAPPED                   (1 << 0)

