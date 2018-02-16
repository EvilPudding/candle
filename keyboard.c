#include <ecm.h>
#include "keyboard.h"
#include <SDL2/SDL.h>

DEC_SIG(key_up);
DEC_SIG(key_down);

/* static char clearModifier(char key, int ctrl) */
/* { */
	/* return key + ctrl * 64; */
/* } */

void keyboard_register()
{
	ecm_register_signal(&key_up, sizeof(char));
	ecm_register_signal(&key_down, sizeof(char));
}

/* void keySpec(const Uint8 *ks, state_t *state) */
/* { */
/* 	if(!ks[SDL_SCANCODE_LSHIFT] && !ks[SDL_SCANCODE_RSHIFT]) */
/* 	{ */
/* 		state->swi = 0; */
/* 		state->shiftdown = 0; */
/* 	} */
/* 	if(state->dev) */
/* 	{ */
/* 		if(!ks[SDL_SCANCODE_LALT] && !ks[SDL_SCANCODE_RALT]) */
/* 		{ */
/* 			state->altdown=0; */
/* 			state->low=0; */
/* 			state->SPA=0; */
/* 		} */
/* 		if(ks[SDL_SCANCODE_LALT] || ks[SDL_SCANCODE_RALT]) */
/* 		{ */
/* 			state->altdown=1; */
/* 			if(state->CH.side || !state->dev) */
/* 			{ */
/* 				state->low=1; */
/* 			} */
/* 			else */
/* 			{ */
/* 				state->SPA=1; */
/* 			} */
/* 		} */
/* 	} */
/*     if(!ks[SDL_SCANCODE_LCTRL] && !ks[SDL_SCANCODE_RCTRL]) */
/*     { */
/*         state->ctrldown=0; */
/*     } */

/*     if(!state->E && (ks[SDL_SCANCODE_LSHIFT] || ks[SDL_SCANCODE_RSHIFT])) */
/*     { */
/*         if(!state->shiftdown) */
/* 		{ */
/* 			state->swi=1; */
/* 		} */
/*         state->shiftdown=1; */
/*     } */
/*     if(ks[SDL_SCANCODE_LCTRL] || ks[SDL_SCANCODE_RCTRL]) */
/*     { */
/*         state->ctrldown=1; */
/*     } */
/* } */

