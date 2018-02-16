#ifndef KEYBOARD_H
#define KEYBOARD_H

void keyboard_register(void);

DEF_SIG(key_up);
DEF_SIG(key_down);

#endif /* !KEYBOARD_H */
