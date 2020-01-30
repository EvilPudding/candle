/*
 * events.h
 * Copyright (C) 2020 pudds <pudds@xpspudim>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef EVENTS_H
#define EVENTS_H

typedef enum
{
	CANDLE_KEYUP,
	CANDLE_KEYDOWN,
	CANDLE_TEXTINPUT,
	CANDLE_MOUSEBUTTONUP,
	CANDLE_MOUSEBUTTONDOWN,
	CANDLE_MOUSEWHEEL,
	CANDLE_MOUSEMOTION,
	CANDLE_CONTROLLERDEVICEADDED,
	CANDLE_CONTROLLERBUTTONDOWN,
	CANDLE_CONTROLLERBUTTONUP,
	CANDLE_CONTROLLERAXISMOTION,
	CANDLE_WINDOW_RESIZE
} candle_event_type_e;

typedef enum
{
	CANDLE_KEY_UNKNOWN                     =  -1,
	CANDLE_KEY_NUL                         =   0,  /* Null char               */
	CANDLE_KEY_SOH                         =   1,  /* Start of Heading        */
	CANDLE_KEY_STX                         =   2,  /* Start of Text           */
	CANDLE_KEY_ETX                         =   3,  /* End of Text             */
	CANDLE_KEY_EOT                         =   4,  /* End of Transmission     */
	CANDLE_KEY_ENQ                         =   5,  /* Enquiry                 */
	CANDLE_KEY_ACK                         =   6,  /* Acknowledgment          */
	CANDLE_KEY_BEL                         =   7,  /* Bell                    */
	CANDLE_KEY_BS                          =   8,  /* Back Space              */
	CANDLE_KEY_HT                          =   9,  /* Horizontal Tab          */
	CANDLE_KEY_LF                          =  10,  /* Line Feed               */
	CANDLE_KEY_VT                          =  11,  /* Vertical Tab            */
	CANDLE_KEY_FF                          =  12,  /* Form Feed               */
	CANDLE_KEY_CR                          =  13,  /* Carriage Return         */
	CANDLE_KEY_SO                          =  14,  /* Shift Out / X-On        */
	CANDLE_KEY_SI                          =  15,  /* Shift In / X-Off        */
	CANDLE_KEY_DLE                         =  16,  /* Data Line Escape        */
	CANDLE_KEY_DC1                         =  17,  /* Device Control 1 (XON)  */
	CANDLE_KEY_DC2                         =  18,  /* Device Control 2        */
	CANDLE_KEY_DC3                         =  19,  /* Device Control 3 (XOFF) */
	CANDLE_KEY_DC4                         =  20,  /* Device Control 4        */
	CANDLE_KEY_NAK                         =  21,  /* Negative Ack            */
	CANDLE_KEY_SYN                         =  22,  /* Synchronous Idle        */
	CANDLE_KEY_ETB                         =  23,  /* End of Transmit Block   */
	CANDLE_KEY_CAN                         =  24,  /* Cancel                  */
	CANDLE_KEY_EM                          =  25,  /* End of Medium           */
	CANDLE_KEY_SUB                         =  26,  /* Substitute              */
	CANDLE_KEY_ESC                         =  27,  /* Escape                  */
	CANDLE_KEY_FS                          =  28,  /* File Separator          */
	CANDLE_KEY_GS                          =  29,  /* Group Separator         */
	CANDLE_KEY_RS                          =  30,  /* Record Separator        */
	CANDLE_KEY_US                          =  31,  /* Unit Separator          */
	CANDLE_KEY_SPACE                       =  32,
	CANDLE_KEY_APOSTROPHE                  =  39,
	CANDLE_KEY_COMMA                       =  44,
	CANDLE_KEY_MINUS                       =  45,
	CANDLE_KEY_PERIOD                      =  46,
	CANDLE_KEY_SLASH                       =  47,
	CANDLE_KEY_0                           =  48,
	CANDLE_KEY_1                           =  49,
	CANDLE_KEY_2                           =  50,
	CANDLE_KEY_3                           =  51,
	CANDLE_KEY_4                           =  52,
	CANDLE_KEY_5                           =  53,
	CANDLE_KEY_6                           =  54,
	CANDLE_KEY_7                           =  55,
	CANDLE_KEY_8                           =  56,
	CANDLE_KEY_9                           =  57,
	CANDLE_KEY_SEMICOLON                   =  59,
	CANDLE_KEY_EQUAL                       =  61,
	CANDLE_KEY_A                           =  65,
	CANDLE_KEY_B                           =  66,
	CANDLE_KEY_C                           =  67,
	CANDLE_KEY_D                           =  68,
	CANDLE_KEY_E                           =  69,
	CANDLE_KEY_F                           =  70,
	CANDLE_KEY_G                           =  71,
	CANDLE_KEY_H                           =  72,
	CANDLE_KEY_I                           =  73,
	CANDLE_KEY_J                           =  74,
	CANDLE_KEY_K                           =  75,
	CANDLE_KEY_L                           =  76,
	CANDLE_KEY_M                           =  77,
	CANDLE_KEY_N                           =  78,
	CANDLE_KEY_O                           =  79,
	CANDLE_KEY_P                           =  80,
	CANDLE_KEY_Q                           =  81,
	CANDLE_KEY_R                           =  82,
	CANDLE_KEY_S                           =  83,
	CANDLE_KEY_T                           =  84,
	CANDLE_KEY_U                           =  85,
	CANDLE_KEY_V                           =  86,
	CANDLE_KEY_W                           =  87,
	CANDLE_KEY_X                           =  88,
	CANDLE_KEY_Y                           =  89,
	CANDLE_KEY_Z                           =  90,
	CANDLE_KEY_LEFT_BRACKET                =  91,
	CANDLE_KEY_BACKSLASH                   =  92,
	CANDLE_KEY_RIGHT_BRACKET               =  93,
	CANDLE_KEY_GRAVE_ACCENT                =  96,
	CANDLE_KEY_a                           =  97,
	CANDLE_KEY_b                           =  98,
	CANDLE_KEY_c                           =  99,
	CANDLE_KEY_d                           = 100,
	CANDLE_KEY_e                           = 101,
	CANDLE_KEY_f                           = 102,
	CANDLE_KEY_g                           = 103,
	CANDLE_KEY_h                           = 104,
	CANDLE_KEY_i                           = 105,
	CANDLE_KEY_j                           = 106,
	CANDLE_KEY_k                           = 107,
	CANDLE_KEY_l                           = 108,
	CANDLE_KEY_m                           = 109,
	CANDLE_KEY_n                           = 110,
	CANDLE_KEY_o                           = 111,
	CANDLE_KEY_p                           = 112,
	CANDLE_KEY_q                           = 113,
	CANDLE_KEY_r                           = 114,
	CANDLE_KEY_s                           = 115,
	CANDLE_KEY_t                           = 116,
	CANDLE_KEY_u                           = 117,
	CANDLE_KEY_v                           = 118,
	CANDLE_KEY_w                           = 119,
	CANDLE_KEY_x                           = 120,
	CANDLE_KEY_y                           = 121,
	CANDLE_KEY_z                           = 122,
	CANDLE_KEY_OPEN_BRACE                  = 123, /* Opening brace */
	CANDLE_KEY_VERT_BAR	                   = 124, /* Vertical bar  */
	CANDLE_KEY_CLOSE_BRACE                 = 125, /* Closing brace */
	CANDLE_KEY_TILDE	                   = 126, /* tilde         */
	CANDLE_KEY_DEL                         = 127, /* Delete        */
	CANDLE_KEY_WORLD_1                     = 161,
	CANDLE_KEY_WORLD_2                     = 162,
	CANDLE_KEY_ESCAPE                      = 256,
	CANDLE_KEY_ENTER                       = 257,
	CANDLE_KEY_TAB                         = 258,
	CANDLE_KEY_BACKSPACE                   = 259,
	CANDLE_KEY_INSERT                      = 260,
	CANDLE_KEY_DELETE                      = 261,
	CANDLE_KEY_RIGHT                       = 262,
	CANDLE_KEY_LEFT                        = 263,
	CANDLE_KEY_DOWN                        = 264,
	CANDLE_KEY_UP                          = 265,
	CANDLE_KEY_PAGE_UP                     = 266,
	CANDLE_KEY_PAGE_DOWN                   = 267,
	CANDLE_KEY_HOME                        = 268,
	CANDLE_KEY_END                         = 269,
	CANDLE_KEY_CAPS_LOCK                   = 280,
	CANDLE_KEY_SCROLL_LOCK                 = 281,
	CANDLE_KEY_NUM_LOCK                    = 282,
	CANDLE_KEY_PRINT_SCREEN                = 283,
	CANDLE_KEY_PAUSE                       = 284,
	CANDLE_KEY_F1                          = 290,
	CANDLE_KEY_F2                          = 291,
	CANDLE_KEY_F3                          = 292,
	CANDLE_KEY_F4                          = 293,
	CANDLE_KEY_F5                          = 294,
	CANDLE_KEY_F6                          = 295,
	CANDLE_KEY_F7                          = 296,
	CANDLE_KEY_F8                          = 297,
	CANDLE_KEY_F9                          = 298,
	CANDLE_KEY_F10                         = 299,
	CANDLE_KEY_F11                         = 300,
	CANDLE_KEY_F12                         = 301,
	CANDLE_KEY_F13                         = 302,
	CANDLE_KEY_F14                         = 303,
	CANDLE_KEY_F15                         = 304,
	CANDLE_KEY_F16                         = 305,
	CANDLE_KEY_F17                         = 306,
	CANDLE_KEY_F18                         = 307,
	CANDLE_KEY_F19                         = 308,
	CANDLE_KEY_F20                         = 309,
	CANDLE_KEY_F21                         = 310,
	CANDLE_KEY_F22                         = 311,
	CANDLE_KEY_F23                         = 312,
	CANDLE_KEY_F24                         = 313,
	CANDLE_KEY_F25                         = 314,
	CANDLE_KEY_KP_0                        = 320,
	CANDLE_KEY_KP_1                        = 321,
	CANDLE_KEY_KP_2                        = 322,
	CANDLE_KEY_KP_3                        = 323,
	CANDLE_KEY_KP_4                        = 324,
	CANDLE_KEY_KP_5                        = 325,
	CANDLE_KEY_KP_6                        = 326,
	CANDLE_KEY_KP_7                        = 327,
	CANDLE_KEY_KP_8                        = 328,
	CANDLE_KEY_KP_9                        = 329,
	CANDLE_KEY_KP_DECIMAL                  = 330,
	CANDLE_KEY_KP_DIVIDE                   = 331,
	CANDLE_KEY_KP_MULTIPLY                 = 332,
	CANDLE_KEY_KP_SUBTRACT                 = 333,
	CANDLE_KEY_KP_ADD                      = 334,
	CANDLE_KEY_KP_ENTER                    = 335,
	CANDLE_KEY_KP_EQUAL                    = 336,
	CANDLE_KEY_LEFT_SHIFT                  = 340,
	CANDLE_KEY_LEFT_CONTROL                = 341,
	CANDLE_KEY_LEFT_ALT                    = 342,
	CANDLE_KEY_LEFT_SUPER                  = 343,
	CANDLE_KEY_RIGHT_SHIFT                 = 344,
	CANDLE_KEY_RIGHT_CONTROL               = 345,
	CANDLE_KEY_RIGHT_ALT                   = 346,
	CANDLE_KEY_RIGHT_SUPER                 = 347,
	CANDLE_KEY_MENU                        = 348,
	CANDLE_MOUSE_BUTTON_LEFT               = 349,
	CANDLE_MOUSE_BUTTON_RIGHT              = 350,
	CANDLE_MOUSE_BUTTON_MIDDLE             = 351,
	CANDLE_CONTROLLER_BUTTON_A             = 352,
	CANDLE_CONTROLLER_BUTTON_B             = 353,
	CANDLE_CONTROLLER_BUTTON_X             = 354,
	CANDLE_CONTROLLER_BUTTON_Y             = 355,
	CANDLE_CONTROLLER_BUTTON_BACK          = 356,
	CANDLE_CONTROLLER_BUTTON_GUIDE         = 357,
	CANDLE_CONTROLLER_BUTTON_START         = 358,
	CANDLE_CONTROLLER_BUTTON_LEFTSTICK     = 359,
	CANDLE_CONTROLLER_BUTTON_RIGHTSTICK    = 360,
	CANDLE_CONTROLLER_BUTTON_LEFTSHOULDER  = 361,
	CANDLE_CONTROLLER_BUTTON_RIGHTSHOULDER = 362,
	CANDLE_CONTROLLER_BUTTON_UP            = 363,
	CANDLE_CONTROLLER_BUTTON_DOWN          = 364,
	CANDLE_CONTROLLER_BUTTON_LEFT          = 365,
	CANDLE_CONTROLLER_BUTTON_RIGHT         = 366,
	CANDLE_CONTROLLER_AXIS_LEFT            = 367,
	CANDLE_CONTROLLER_AXIS_RIGHT           = 369,
	CANDLE_CONTROLLER_TRIGGERLEFT          = 371,
	CANDLE_CONTROLLER_TRIGGERRIGHT         = 372
} candle_key_e; 

typedef struct
{
	int id;
	int value;
} candle_event_controller_t;

typedef struct
{
	float x, y;
	int direction;
} candle_event_wheel_t;

typedef struct
{
	int x, y;
} candle_event_mouse_t;

typedef struct
{
	int width, height;
} candle_event_window_t;

typedef struct
{
	candle_event_type_e type;
	candle_key_e key;
	candle_event_controller_t controller;
	candle_event_window_t window;
	candle_event_wheel_t wheel;
	candle_event_mouse_t mouse;
} candle_event_t;


#endif /* !EVENTS_H */
