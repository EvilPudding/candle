#ifndef KEYBOARD_H
#define KEYBOARD_H

void keyboard_register(ecm_t *ecm);

extern unsigned long key_up;
extern unsigned long key_down;

#endif /* !KEYBOARD_H */
