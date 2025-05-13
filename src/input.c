#include <windows.h>

#include <stdio.h>
#include <stdint.h>

#include <SDL2/SDL.h>

#include <global.h>
#include <patch.h>
#include <config.h>
#include <script.h>

// device is +0x100
typedef struct {
	uint32_t vtablePtr;
	//uint32_t node;
	uint32_t type;
	uint32_t port;
	// 16
	uint32_t slot;
	uint32_t isValid;
	uint32_t unk_24;
	uint8_t controlData[32];	// PS2 format control data
	uint8_t vibrationData_align[32];
	uint8_t vibrationData_direct[32];
	uint8_t vibrationData_max[32];
	uint8_t vibrationData_oldDirect[32];    // there may be something before this
	// 160
	// 176
	//uint32_t unk4;
	//uint32_t unk4;
	//uint32_t unk4;
	uint8_t unkChunk[100];
	uint32_t unk5;
	// 192
	uint32_t unk6;
	uint32_t actuatorsDisabled;	// +0x124
	uint32_t capabilities;	// +0x128
	uint32_t unk7;	// +0x12c
	// 208
	uint32_t num_actuators; // +0x130
	uint32_t unk8;	// +0x134
	uint32_t unk9;	// +0x138
	uint32_t state;	// +0x13c
	uint32_t test;	// +0x140
	// 224
	uint32_t index;	// CORRECT HERE!!	+0x144
	uint32_t isPluggedIn;	// +0x148
	uint32_t unplugged_counter;
	uint32_t unplugged_retry;

	uint32_t pressed;
	uint32_t start_or_a_pressed;
	// 238
} device;

void patchPs2Buttons();

int controllerCount;
int controllerListSize;
SDL_GameController **controllerList;
struct inputsettings inputsettings;
struct keybinds keybinds;
struct controllerbinds padbinds;

uint8_t isUsingKeyboard = 1;

struct playerslot {
	SDL_GameController *controller;
	uint8_t lockedOut;	// after sign-in, a controller is "locked out" until all of its buttons are released
};

#define MAX_PLAYERS 2
uint8_t numplayers = 0;
struct playerslot players[MAX_PLAYERS] = { { NULL, 0 }, { NULL, 0 } };

void setUsingKeyboard(uint8_t usingKeyboard) {
	isUsingKeyboard = usingKeyboard;
}

void addController(int idx) {
	printf("Detected controller \"%s\"\n", SDL_GameControllerNameForIndex(idx));

	SDL_GameController *controller = SDL_GameControllerOpen(idx);

	SDL_GameControllerSetPlayerIndex(controller, -1);

	if (controller) {
		if (controllerCount == controllerListSize) {
			int tmpSize = controllerListSize + 1;
			SDL_GameController **tmp = realloc(controllerList, sizeof(SDL_GameController *) * tmpSize);
			if (!tmp) {
				return; // TODO: log something here or otherwise do something
			}

			controllerListSize = tmpSize;
			controllerList = tmp;
		}

		controllerList[controllerCount] = controller;
		controllerCount++;
	}
}

void addplayer(SDL_GameController *controller) {
	if (numplayers < 2) {
		// find open slot
		uint8_t found = 0;
		int i = 0;
		for (; i < MAX_PLAYERS; i++) {
			if (!players[i].controller) {
				found = 1;
				break;
			}
		}
		if (found) {
			SDL_GameControllerSetPlayerIndex(controller, i);
			players[i].controller = controller;

			if (numplayers > 0) {
				players[i].lockedOut = 1;
				SDL_JoystickRumble(SDL_GameControllerGetJoystick(controller), 0x7fff, 0x7fff, 250);
			}
			
			numplayers++;

			printf("Added player %d: %s\n", i + 1, SDL_GameControllerName(controller));
		}
	} else {
		printf("Already two players, not adding\n");
	}
}

void pruneplayers() {
	for (int i = 0; i < MAX_PLAYERS; i++) {
		if (players[i].controller && !SDL_GameControllerGetAttached(players[i].controller)) {
			printf("Pruned player %d\n", i + 1);

			players[i].controller = NULL;
			numplayers--;
			printf("Remaining players: %d\n", numplayers);
		}
	}
}

void removeController(SDL_GameController *controller) {
	printf("Controller \"%s\" disconnected\n", SDL_GameControllerName(controller));

	int i = 0;

	while (i < controllerCount && controllerList[i] != controller) {
		i++;
	}

	if (controllerList[i] == controller) {
		SDL_GameControllerClose(controller);

		int playerIdx = SDL_GameControllerGetPlayerIndex(controller);
		if (playerIdx != -1) {
			printf("Removed player %d\n", playerIdx + 1);
			players[playerIdx].controller = NULL;
			numplayers--;
		}

		pruneplayers();

		for (; i < controllerCount - 1; i++) {
			controllerList[i] = controllerList[i + 1];
		}
		controllerCount--;
	} else {
		//setActiveController(NULL);
		printf("Did not find disconnected controller in list\n");
	}
}

void initSDLControllers() {
	printf("Initializing Controller Input\n");

	controllerCount = 0;
	controllerListSize = 1;
	controllerList = malloc(sizeof(SDL_GameController *) * controllerListSize);

	for (int i = 0; i < SDL_NumJoysticks(); i++) {
		if (SDL_IsGameController(i)) {
			addController(i);
		}
	}

	// add event filter for newly connected controllers
	//SDL_SetEventFilter(controllerEventFilter, NULL);
}

uint8_t axisAbs(uint8_t val) {
	if (val > 127) {
		// positive, just remove top bit
		return val & 0x7F;
	} else {
		// negative
		return ~val & 0x7f;
	}
}

uint8_t getButton(SDL_GameController *controller, controllerButton button) {
	if (button == CONTROLLER_BUTTON_LEFTTRIGGER) {
		uint8_t pressure = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT) >> 7;
		return pressure > 0x80;
	} else if (button == CONTROLLER_BUTTON_RIGHTTRIGGER) {
		uint8_t pressure = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) >> 7;
		return pressure > 0x80;
	} else {
		return SDL_GameControllerGetButton(controller, button);
	}
}

void getStick(SDL_GameController *controller, controllerStick stick, uint8_t *xOut, uint8_t *yOut) {
	uint8_t result_x, result_y;

	if (stick == CONTROLLER_STICK_LEFT) {
		result_x = (uint8_t)((SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX) >> 8) + 128);
		result_y = (uint8_t)((SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY) >> 8) + 128);
	} else if (stick == CONTROLLER_STICK_RIGHT) {
		result_x = (uint8_t)((SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTX) >> 8) + 128);
		result_y = (uint8_t)((SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTY) >> 8) + 128);
	} else {
		result_x = 0x80;
		result_y = 0x80;
	}

	if (axisAbs(result_x) > axisAbs(*xOut)) {
		*xOut = result_x;
	}

	if (axisAbs(result_y) > axisAbs(*yOut)) {
		*yOut = result_y;
	}
}

uint8_t* addr_isKeyboardOnScreen = 0x00ab5bae;
uint8_t *addr_isMenuOnScreen = 0x00ab5bac;

uint8_t isInMenu = 0;

void pollController(device *dev, SDL_GameController *controller) {
	if (SDL_GameControllerGetAttached(controller)) {
		dev->isValid = 1;
		dev->isPluggedIn = 1;

		// buttons
		if (getButton(controller, padbinds.menu)) {
			dev->controlData[2] |= 0x01 << 3;
		}
		if (getButton(controller, padbinds.cameraToggle)) {
			dev->controlData[2] |= 0x01 << 0;
		}
		if (getButton(controller, padbinds.cameraSwivelLock)) {
			dev->controlData[2] |= 0x01 << 2;
		}
		/*if (getButton(controller, padbinds.focus)) {
			dev->controlData[2] |= 0x01 << 1;
		}*/

		if (getButton(controller, padbinds.grind)) {
			dev->controlData[3] |= 0x01 << 4;
			dev->controlData[12] = 0xff;
		}
		if (getButton(controller, padbinds.grab)) {
			dev->controlData[3] |= 0x01 << 5;
			dev->controlData[13] = 0xff;
		}
		if (getButton(controller, padbinds.ollie)) {
			dev->controlData[3] |= 0x01 << 6;
			dev->controlData[14] = 0xff;
		}
		if (getButton(controller, padbinds.kick)) {
			dev->controlData[3] |= 0x01 << 7;
			dev->controlData[15] = 0xff;
		}

		// shoulders
		if (inputsettings.isPs2Controls) {
			if (getButton(controller, padbinds.leftSpin)) {
				dev->controlData[3] |= 0x01 << 2;
				dev->controlData[16] = 0xff;
			}

			if (getButton(controller, padbinds.rightSpin)) {
				dev->controlData[3] |= 0x01 << 3;
				dev->controlData[17] = 0xff;
			}

			if (getButton(controller, padbinds.nollie)) {
				dev->controlData[3] |= 0x01 << 0;
				dev->controlData[18] = 0xff;
			}

			if (getButton(controller, padbinds.switchRevert)) {
				dev->controlData[3] |= 0x01 << 1;
				dev->controlData[19] = 0xff;
			}
		} else {
			if (getButton(controller, padbinds.leftSpin)) {
				dev->controlData[3] |= 0x01 << 2;
				dev->controlData[16] = 0xff;
				dev->controlData[3] |= 0x01 << 0;
				dev->controlData[18] = 0xff;
			}

			if (getButton(controller, padbinds.rightSpin)) {
				dev->controlData[3] |= 0x01 << 3;
				dev->controlData[17] = 0xff;
				dev->controlData[3] |= 0x01 << 1;
				dev->controlData[19] = 0xff;
			}

			if (getButton(controller, padbinds.switchRevert)) {
				dev->controlData[3] |= 0x01 << 3;
				dev->controlData[17] = 0xff;
				dev->controlData[3] |= 0x01 << 1;
				dev->controlData[19] = 0xff;
				dev->controlData[3] |= 0x01 << 2;
				dev->controlData[16] = 0xff;
				dev->controlData[3] |= 0x01 << 0;
				dev->controlData[18] = 0xff;
			}
		}

		
		// d-pad
		if (SDL_GameControllerGetButton(controller, padbinds.up)) {
			dev->controlData[2] |= 0x01 << 4;
			dev->controlData[10] = 0xFF;
		}
		if (SDL_GameControllerGetButton(controller, padbinds.right)) {
			dev->controlData[2] |= 0x01 << 5;
			dev->controlData[8] = 0xFF;
		}
		if (SDL_GameControllerGetButton(controller, padbinds.down)) {
			dev->controlData[2] |= 0x01 << 6;
			dev->controlData[11] = 0xFF;
		}
		if (SDL_GameControllerGetButton(controller, padbinds.left)) {
			dev->controlData[2] |= 0x01 << 7;
			dev->controlData[9] = 0xFF;
		}
		
		// sticks
		getStick(controller, padbinds.camera, &(dev->controlData[4]), &(dev->controlData[5]));
		getStick(controller, padbinds.movement, &(dev->controlData[6]), &(dev->controlData[7]));
	}
}

uint8_t prev_in_menu = 0;
uint8_t prev_in_keyboard = 0;

#define LOCKOUT 2
struct menuKeys {
	uint8_t affirmative;	// a/x
	uint8_t negative;	// b/circle
	uint8_t	menu;	// x/square
	uint8_t extra;	// y/triangle
	uint8_t up;
	uint8_t down;
	uint8_t left;
	uint8_t right;
	uint8_t rot_left;	// lb/l1
	uint8_t rot_right;	// rb/r1
	uint8_t zoom;	// black (also r2, but that's rerouted to black)
	// not handling graphic editor transforms now, it is too likely to interfere with desired menu binds
	/*
	uint8_t transform_rot_left;	// cam left
	uint8_t transform_rot_right;	// cam right
	uint8_t transform_scale_up;	// cam up
	uint8_t transform_scale_down;	// cam down
	*/
};

struct menuKeys menuKeyStates;

void initMenuKeyStates() {
	memset(&menuKeyStates, 0, sizeof(struct menuKeys));
}

void processMenuBinds(uint8_t *keyStates) {
	if (!inputsettings.useKeyboardControls) {
		return;
	}

	// if menu state is changing 
	//printf("keyboard_on_screen: %d, prev: %d\n", *addr_isKeyboardOnScreen, prev_in_keyboard);
	if (isInMenu != prev_in_menu || *addr_isKeyboardOnScreen != prev_in_keyboard) {
		if (keybinds.ollie != SDL_SCANCODE_RETURN && menuKeyStates.affirmative == 1) {
			menuKeyStates.affirmative = LOCKOUT;
		}
		if (keybinds.grab != SDL_SCANCODE_ESCAPE && menuKeyStates.negative == 1) {
			menuKeyStates.negative = LOCKOUT;
		}
		if (keybinds.kick != SDL_SCANCODE_E && menuKeyStates.menu == 1) {
			menuKeyStates.menu = LOCKOUT;
		}
		if (keybinds.grind != SDL_SCANCODE_R && menuKeyStates.extra == 1) {
			menuKeyStates.extra = LOCKOUT;
		}
		if (keybinds.up != SDL_SCANCODE_UP && menuKeyStates.up == 1) {
			menuKeyStates.up = LOCKOUT;
		}
		if (keybinds.down != SDL_SCANCODE_DOWN && menuKeyStates.down == 1) {
			menuKeyStates.down = LOCKOUT;
		}
		if (keybinds.left != SDL_SCANCODE_LEFT && menuKeyStates.left == 1) {
			menuKeyStates.left = LOCKOUT;
		}
		if (keybinds.right != SDL_SCANCODE_RIGHT && menuKeyStates.right == 1) {
			menuKeyStates.right = LOCKOUT;
		}
		if (inputsettings.isPs2Controls) {
			if (keybinds.leftSpin != SDL_SCANCODE_1 && menuKeyStates.rot_left == 1) {
				menuKeyStates.rot_left = LOCKOUT;
			}
			if (keybinds.rightSpin != SDL_SCANCODE_2 && menuKeyStates.rot_right == 1) {
				menuKeyStates.rot_right = LOCKOUT;
			}
			if (keybinds.switchRevert != SDL_SCANCODE_MINUS && menuKeyStates.zoom == 1) {
				menuKeyStates.zoom = LOCKOUT;
			}
		}
		else {
			if (keybinds.nollie != SDL_SCANCODE_1 && menuKeyStates.rot_left == 1) {
				menuKeyStates.rot_left = LOCKOUT;
			}
			if (keybinds.switchRevert != SDL_SCANCODE_2 && menuKeyStates.rot_right == 1) {
				menuKeyStates.rot_right = LOCKOUT;
			}
			if (keybinds.rightSpin != SDL_SCANCODE_MINUS && menuKeyStates.zoom == 1) {
				menuKeyStates.zoom = LOCKOUT;
			}
		}

		prev_in_menu = isInMenu;
		prev_in_keyboard = *addr_isKeyboardOnScreen;
	}

	// process keys, clear lockout for any keys that are up
	if (!(menuKeyStates.affirmative == LOCKOUT && keyStates[SDL_SCANCODE_RETURN])) {
		menuKeyStates.affirmative = keyStates[SDL_SCANCODE_RETURN];
	}
	if (!(menuKeyStates.negative == LOCKOUT && keyStates[SDL_SCANCODE_ESCAPE])) {
		menuKeyStates.negative = keyStates[SDL_SCANCODE_ESCAPE];
	}
	if (!(menuKeyStates.menu == LOCKOUT && keyStates[SDL_SCANCODE_E])) {
		menuKeyStates.menu = keyStates[SDL_SCANCODE_E];
	}
	if (!(menuKeyStates.extra == LOCKOUT && keyStates[SDL_SCANCODE_R])) {
		menuKeyStates.extra = keyStates[SDL_SCANCODE_R];
	}
	if (!(menuKeyStates.up == LOCKOUT && keyStates[SDL_SCANCODE_UP])) {
		menuKeyStates.up = keyStates[SDL_SCANCODE_UP];
	}
	if (!(menuKeyStates.down == LOCKOUT && keyStates[SDL_SCANCODE_DOWN])) {
		menuKeyStates.down = keyStates[SDL_SCANCODE_DOWN];
	}
	if (!(menuKeyStates.left == LOCKOUT && keyStates[SDL_SCANCODE_LEFT])) {
		menuKeyStates.left = keyStates[SDL_SCANCODE_LEFT];
	}
	if (!(menuKeyStates.right == LOCKOUT && keyStates[SDL_SCANCODE_RIGHT])) {
		menuKeyStates.right = keyStates[SDL_SCANCODE_RIGHT];
	}
	if (!(menuKeyStates.rot_left == LOCKOUT && keyStates[SDL_SCANCODE_1])) {
		menuKeyStates.rot_left = keyStates[SDL_SCANCODE_1];
	}
	if (!(menuKeyStates.rot_right == LOCKOUT && keyStates[SDL_SCANCODE_2])) {
		menuKeyStates.rot_right = keyStates[SDL_SCANCODE_2];
	}
	if (!(menuKeyStates.zoom == LOCKOUT && keyStates[SDL_SCANCODE_MINUS])) {
		menuKeyStates.zoom = keyStates[SDL_SCANCODE_MINUS];
	}
}

uint8_t getKeyState(uint8_t *keyStates, uint8_t key) {
	switch (key) {
	case SDL_SCANCODE_RETURN:
		return menuKeyStates.affirmative != LOCKOUT && (!isInMenu && keyStates[key]);
	case SDL_SCANCODE_ESCAPE:
		return menuKeyStates.negative != LOCKOUT && (!isInMenu && keyStates[key]);
	case SDL_SCANCODE_E:
		return menuKeyStates.menu != LOCKOUT && (!isInMenu && keyStates[key]);
	case SDL_SCANCODE_R:
		return menuKeyStates.extra != LOCKOUT && (!isInMenu && keyStates[key]);
	case SDL_SCANCODE_UP:
		return menuKeyStates.up != LOCKOUT && (!isInMenu && keyStates[key]);
	case SDL_SCANCODE_DOWN:
		return menuKeyStates.down != LOCKOUT && (!isInMenu && keyStates[key]);
	case SDL_SCANCODE_LEFT:
		return menuKeyStates.left != LOCKOUT && (!isInMenu && keyStates[key]);
	case SDL_SCANCODE_RIGHT:
		return menuKeyStates.right != LOCKOUT && (!isInMenu && keyStates[key]);
	case SDL_SCANCODE_1:
		return menuKeyStates.rot_left != LOCKOUT && (!isInMenu && keyStates[key]);
	case SDL_SCANCODE_2:
		return menuKeyStates.rot_right != LOCKOUT && (!isInMenu && keyStates[key]);
	case SDL_SCANCODE_MINUS:
		return menuKeyStates.zoom != LOCKOUT && (!isInMenu && keyStates[key]);
	default:
		return keyStates[key];
	}
}

void pollKeyboard(device* dev) {
	dev->isValid = 1;
	dev->isPluggedIn = 1;

	uint8_t* keyboardState = SDL_GetKeyboardState(NULL);

	processMenuBinds(keyboardState);

	if (*addr_isKeyboardOnScreen && inputsettings.useKeyboardControls) {
		return;
	}

	// buttons
	if (getKeyState(keyboardState, keybinds.menu)) {
		dev->controlData[2] |= 0x01 << 3;
	}
	if (getKeyState(keyboardState, keybinds.cameraToggle)) {
		dev->controlData[2] |= 0x01 << 0;
	}
	if (getKeyState(keyboardState, keybinds.cameraSwivelLock)) {
		dev->controlData[2] |= 0x01 << 2;
	}

	if (getKeyState(keyboardState, keybinds.grind) || (isInMenu && menuKeyStates.extra == 1)) {
		dev->controlData[3] |= 0x01 << 4;
		dev->controlData[12] = 0xff;
	}
	if (getKeyState(keyboardState, keybinds.grab) || (isInMenu && menuKeyStates.negative == 1)) {
		dev->controlData[3] |= 0x01 << 5;
		dev->controlData[13] = 0xff;
	}
	if (getKeyState(keyboardState, keybinds.ollie) || (isInMenu && menuKeyStates.affirmative == 1)) {
		dev->controlData[3] |= 0x01 << 6;
		dev->controlData[14] = 0xff;
	}
	if (getKeyState(keyboardState, keybinds.kick) || (isInMenu && menuKeyStates.menu == 1)) {
		dev->controlData[3] |= 0x01 << 7;
		dev->controlData[15] = 0xff;
	}

	// shoulders
	if (inputsettings.isPs2Controls) {
		if (getKeyState(keyboardState, keybinds.leftSpin) || (isInMenu && menuKeyStates.rot_left == 1)) {
			dev->controlData[3] |= 0x01 << 2;
			dev->controlData[16] = 0xff;
		}
		if (getKeyState(keyboardState, keybinds.rightSpin) || (isInMenu && menuKeyStates.rot_right == 1)) {
			dev->controlData[3] |= 0x01 << 3;
			dev->controlData[17] = 0xff;
		}
		if (getKeyState(keyboardState, keybinds.nollie)) {
			dev->controlData[3] |= 0x01 << 0;
			dev->controlData[18] = 0xff;
			if (isInMenu) {
				dev->controlData[20] |= 0x01 << 1;	// act as white when in menu
			}
		}
		if (getKeyState(keyboardState, keybinds.switchRevert) || (isInMenu && menuKeyStates.zoom == 1)) {
			dev->controlData[3] |= 0x01 << 1;
			dev->controlData[19] = 0xff;
			if (isInMenu) {
				dev->controlData[20] |= 0x01 << 0;	// act as black when in menu
			}
		}
	}
	else {
		if (!(*addr_isKeyboardOnScreen)) {
			// when editing a park, right trigger acts as l1
			if (getKeyState(keyboardState, keybinds.switchRevert)) {
				dev->controlData[3] |= 0x01 << 2;
				dev->controlData[16] = 0xff;
			}

			if (getKeyState(keyboardState, keybinds.nollie)) {
				dev->controlData[3] |= 0x01 << 0;
				dev->controlData[18] = 0xff;
			}

			if (getKeyState(keyboardState, keybinds.leftSpin)) {
				dev->controlData[20] |= 0x01 << 1;
			}
			if (getKeyState(keyboardState, keybinds.rightSpin)) {
				dev->controlData[20] |= 0x01 << 0;
			}
		}
		else {
			if (getKeyState(keyboardState, keybinds.nollie) || (isInMenu && menuKeyStates.rot_left == 1)) {
				dev->controlData[3] |= 0x01 << 2;
				dev->controlData[16] = 0xff;
				dev->controlData[3] |= 0x01 << 0;
				dev->controlData[18] = 0xff;
			}
			if (getKeyState(keyboardState, keybinds.switchRevert) || (isInMenu && menuKeyStates.rot_right == 1)) {
				dev->controlData[3] |= 0x01 << 3;
				dev->controlData[17] = 0xff;
				dev->controlData[3] |= 0x01 << 1;
				dev->controlData[19] = 0xff;
			}
			if (getKeyState(keyboardState, keybinds.leftSpin)) {
				dev->controlData[20] |= 0x01 << 1;
			}
			if (getKeyState(keyboardState, keybinds.rightSpin) || (isInMenu && menuKeyStates.zoom == 1)) {
				dev->controlData[20] |= 0x01 << 0;
			}
		}
	}

	// d-pad
	if (getKeyState(keyboardState, keybinds.item_up) || (isInMenu && menuKeyStates.up == 1)) {
		dev->controlData[2] |= 0x01 << 4;
		dev->controlData[10] = 0xFF;
	}
	if (getKeyState(keyboardState, keybinds.item_right) || (isInMenu && menuKeyStates.right == 1)) {
		dev->controlData[2] |= 0x01 << 5;
		dev->controlData[8] = 0xFF;
	}
	if (getKeyState(keyboardState, keybinds.item_down) || (isInMenu && menuKeyStates.down == 1)) {
		dev->controlData[2] |= 0x01 << 6;
		dev->controlData[11] = 0xFF;
	}
	if (getKeyState(keyboardState, keybinds.item_left) || (isInMenu && menuKeyStates.left == 1)) {
		dev->controlData[2] |= 0x01 << 7;
		dev->controlData[9] = 0xFF;
	}

	// sticks - NOTE: because these keys are very rarely used/important, SOCD handling is just to cancel
	// right
	// x
	if (getKeyState(keyboardState, keybinds.cameraLeft) && !getKeyState(keyboardState, keybinds.cameraRight)) {
		dev->controlData[4] = 0;
	}
	if (getKeyState(keyboardState, keybinds.cameraRight) && !getKeyState(keyboardState, keybinds.cameraLeft)) {
		dev->controlData[4] = 255;
	}

	// y
	if (getKeyState(keyboardState, keybinds.cameraUp) && !getKeyState(keyboardState, keybinds.cameraDown)) {
		dev->controlData[5] = 0;
	}
	if (getKeyState(keyboardState, keybinds.cameraDown) && !getKeyState(keyboardState, keybinds.cameraUp)) {
		dev->controlData[5] = 255;
	}

	// left
	// x
	if (getKeyState(keyboardState, keybinds.left) && !getKeyState(keyboardState, keybinds.right)) {
		dev->controlData[6] = 0;
	}
	if (getKeyState(keyboardState, keybinds.right) && !getKeyState(keyboardState, keybinds.left)) {
		dev->controlData[6] = 255;
	}

	// y
	if (getKeyState(keyboardState, keybinds.up) && !getKeyState(keyboardState, keybinds.down)) {
		dev->controlData[7] = 0;
	}
	if (getKeyState(keyboardState, keybinds.down) && !getKeyState(keyboardState, keybinds.up)) {
		dev->controlData[7] = 255;
	}
}

// returns 1 if a text entry prompt is on-screen so that keybinds don't interfere with text entry confirmation/cancellation
uint8_t isKeyboardTyping() {
	uint8_t *keyboard_on_screen = 0x00ab5bae;

	return *keyboard_on_screen;
}

void do_key_input(SDL_KeyCode key) {
	void (*key_input)(int32_t key, uint32_t param) = (void *)0x005426c0;
	void (*key_input_arrow)(int32_t key, uint32_t param) = (void *)0x00542730;
	//uint8_t *keyboard_on_screen = 0x00ab5bac;

	//if (!*keyboard_on_screen) {
	//	return;
	//}

	if (!inputsettings.useKeyboardControls) {
		return;
	}

	int32_t key_out = 0;
	uint8_t modstate = SDL_GetModState();
	uint8_t shift = SDL_GetModState() & KMOD_SHIFT;
	uint8_t caps = SDL_GetModState() & KMOD_CAPS;

	// send arrow key inputs to other event
	if (key >= SDLK_RIGHT && key <= SDLK_UP) {
		switch(key) {
		case SDLK_RIGHT:
			key_out = 0x27;
			break;
		case SDLK_LEFT:
			key_out = 0x25;
			break;
		case SDLK_DOWN:
			key_out = 0x28;
			break;
		case SDLK_UP:
			key_out = 0x26;
			break;
		}

		key_input_arrow(key_out, 0);
		return;
	}

	if (!*addr_isKeyboardOnScreen && !isInMenu) {
		if (key == SDLK_RETURN) {
			key_input(0x19, 0);
		}
		else if (key == SDLK_F1) {
			key_input(0x1f, 0);
		}
		else if (key == SDLK_F2) {
			key_input(0x1c, 0);
		}
		else if (key == SDLK_F3) {
			key_input(0x1d, 0);
		}
		else if (key == SDLK_F4) {
			key_input(0x1e, 0);
		}

		return;
	}

	if (*addr_isKeyboardOnScreen) {
		if (key == SDLK_RETURN && menuKeyStates.affirmative != LOCKOUT) {
			key_out = 0x0d;	// CR
		}
		else if (key == SDLK_BACKSPACE) {
			key_out = 0x08;	// BS
		}
		else if (key == SDLK_ESCAPE && menuKeyStates.negative != LOCKOUT) {
			key_out = 0x1b;	// ESC
		}
		else if (key == SDLK_KP_ENTER && menuKeyStates.affirmative != LOCKOUT) {
			key_out = 0x0d;
		}
		else {
			key_out = -1;
		}

		if (key_out != -1) {
			key_input(key_out, 0);
		}
	}
}

void do_text_input(char* text) {
	void (*key_input)(int32_t key, uint32_t param) = (void*)0x005426c0;
	if (*addr_isKeyboardOnScreen && inputsettings.useKeyboardControls) {
		if (strlen(text) == 1) {
			key_input(text[0], 0);
		}
		else {
			printf("Input text '%s' > 1 byte!!\n");
		}
	}
}

void processEvent(SDL_Event *e) {
	switch (e->type) {
		case SDL_CONTROLLERDEVICEADDED:
			if (SDL_IsGameController(e->cdevice.which)) {
				addController(e->cdevice.which);
			} else {
				printf("Not a game controller: %s\n", SDL_JoystickNameForIndex(e->cdevice.which));
			}
			return;
		case SDL_CONTROLLERDEVICEREMOVED: {
			SDL_GameController *controller = SDL_GameControllerFromInstanceID(e->cdevice.which);
			if (controller) {
				removeController(controller);
			}
			return;
		}
		case SDL_JOYDEVICEADDED:
			printf("Joystick added: %s\n", SDL_JoystickNameForIndex(e->jdevice.which));
			return;
		case SDL_KEYDOWN: 
			setUsingKeyboard(1);
			do_key_input(e->key.keysym.sym);
			return;
		case SDL_CONTROLLERBUTTONDOWN: {
			SDL_GameController *controller = SDL_GameControllerFromInstanceID(e->cdevice.which);

			int idx = SDL_GameControllerGetPlayerIndex(controller);
			if (idx == -1) {
				addplayer(controller);
			} else if (players[idx].lockedOut) {
				players[idx].lockedOut++;
			}

			setUsingKeyboard(0);
			return;
		}
		case SDL_CONTROLLERBUTTONUP: {
			SDL_GameController *controller = SDL_GameControllerFromInstanceID(e->cdevice.which);

			int idx = SDL_GameControllerGetPlayerIndex(controller);
			if (idx != -1 && players[idx].lockedOut) {
				players[idx].lockedOut--;
			}

			return;
		}
		case SDL_CONTROLLERAXISMOTION:
			setUsingKeyboard(0);
			return;
		case SDL_TEXTINPUT:
			do_text_input(e->text.text);
			return;
		case SDL_WINDOWEVENT:
			if (e->window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
				uint8_t *isFocused = (uint8_t *)0x005a027c;
				*isFocused = 0;
			} else if (e->window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
				uint8_t *recreateDevice = (uint8_t *)0x00aab48e;
				*recreateDevice = 1;

				uint8_t *isFocused = (uint8_t *)0x005a027c;
				*isFocused = 1;
			}
			return;
		case SDL_QUIT: {
			int *shouldQuit = 0x00aab48c;
			*shouldQuit = 1;
			return;
		}
		default:
			return;
	}
}

void processEventsUnfocused() {
	// called when window is unfocused so that window events are still processed

	SDL_Event e;
	while(SDL_PollEvent(&e)) {
		processEvent(&e);
	}
}

void __cdecl processController(device *dev) {
	// cheating:
	// replace type with index
	//dev->type = 60;
	uint8_t *otherIsInMenu = 0x00ab752d;
	//printf("F: 0x%02x, D: 0x%02x\n", *test1, *test2);

	isInMenu = (*addr_isMenuOnScreen || *otherIsInMenu) && inputsettings.useKeyboardControls;

	//printf("IS KEYBOARD ON SCREEN: %d\n", isKeyboardTyping());
	//printf("Processing Controller %d %d %d!\n", dev->index, dev->slot, dev->port);
	//printf("TYPE: %d\n", dev->type);
	//printf("ISPLUGGEDIN: %d\n", dev->isPluggedIn);
	dev->capabilities = 0x0003;
	dev->num_actuators = 2;
	dev->vibrationData_max[0] = 255;
	dev->vibrationData_max[1] = 255;
	dev->state = 2;
	//dev->actuatorsDisabled = 0;

	SDL_Event e;
	while(SDL_PollEvent(&e)) {
		processEvent(&e);
		//printf("EVENT!!!\n");
	}

	dev->isValid = 0;
	dev->isPluggedIn = 0;

	dev->controlData[0] = 0;
	dev->controlData[1] = 0x70;

	// buttons bitmap
	// this bitmap may not work how you would expect. each bit is 1 if the button is *up*, not down
	// the original code sets this initial value at 0xff and XOR's each button bit with the map
	// this scheme cannot be continuously composited, so instead we OR each button with the bitmap and bitwise negate it after all controllers are processed
	dev->controlData[2] = 0x00;
	dev->controlData[3] = 0x00;

	// buttons
	dev->controlData[12] = 0;
	dev->controlData[13] = 0;
	dev->controlData[14] = 0;
	dev->controlData[15] = 0;

	// shoulders
	dev->controlData[16] = 0;
	dev->controlData[17] = 0;
	dev->controlData[18] = 0;
	dev->controlData[19] = 0;

	// d-pad
	dev->controlData[8] = 0;
	dev->controlData[9] = 0;
	dev->controlData[10] = 0;
	dev->controlData[11] = 0;

	// sticks
	dev->controlData[4] = 127;
	dev->controlData[5] = 127;
	dev->controlData[6] = 127;
	dev->controlData[7] = 127;

	dev->controlData[20] = 0;

	if (dev->port == 0) {
		dev->isValid = 1;
		dev->isPluggedIn = 1;

		pollKeyboard(dev);
	}
	
	if (dev->port < MAX_PLAYERS) {
		if (players[dev->port].controller && !players[dev->port].lockedOut) {
			pollController(dev, players[dev->port].controller);
		}
	}

	dev->controlData[2] = ~dev->controlData[2];
	dev->controlData[3] = ~dev->controlData[3];
	//dev->controlData[20] = ~dev->controlData[20];

	if (0xFFFF ^ ((dev->controlData[2] << 8 ) | dev->controlData[3])) {
		dev->pressed = 1;
	} else {
		dev->pressed = 0;
	}

	if (~dev->controlData[2] & 0x01 << 3 || ~dev->controlData[3] & 0x01 << 6) {
		dev->start_or_a_pressed = 1;
	} else {
		dev->start_or_a_pressed = 0;
	}
}

void __cdecl set_actuators(device *dev, uint16_t high, uint16_t low) {
	dev = ((uint8_t *)dev) - 4;
	//printf("SETTING ACTUATORS: %d %d %d - %08x %d %d %d\n", dev->port, left, right, &dev->num_actuators, dev->num_actuators, dev->vibrationData_direct[0], dev->vibrationData_direct[1]);
	for (int i = 0; i < controllerCount; i++) {
		if (SDL_GameControllerGetAttached(controllerList[i]) && SDL_GameControllerGetPlayerIndex(controllerList[i]) == dev->port) {
			SDL_JoystickRumble(SDL_GameControllerGetJoystick(controllerList[i]), low, high, 0);
		}
	}
}

void __fastcall activate_actuators(device *dev, void *pad, int idx, int pct) {
	if (dev->actuatorsDisabled) {
		return;
	}

	//printf("ACTIVATING ACTUATOR: %d, %d\n", idx, pct);

	if (dev->state == 2 && (dev->capabilities & 2) && idx >= 0 && idx < dev->num_actuators) {
		float str = ((float)pct * 0.01f) * dev->vibrationData_max[idx];
		dev->vibrationData_direct[idx] = str;

		uint16_t high = ((uint16_t)dev->vibrationData_direct[0]) << 8;
		uint16_t low = ((uint16_t)dev->vibrationData_direct[1]) << 8;

		set_actuators(((uint8_t *)dev) + 4, high, low);

	}
}

void __stdcall initManager() {
	printf("Initializing Manager!\n");

	// init sdl here
	SDL_Init(SDL_INIT_GAMECONTROLLER);

	//SDL_SetHint(SDL_HINT_HIDAPI_IGNORE_DEVICES, "0x1ccf/0x0000");
	SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "false");

	char *controllerDbPath[filePathBufLen];
	int result = sprintf_s(controllerDbPath, filePathBufLen,"%s%s", executableDirectory, "gamecontrollerdb.txt");

	if (result) {
		result = SDL_GameControllerAddMappingsFromFile(controllerDbPath);
		if (result) {
			printf("Loaded mappings\n");
		} else {
			printf("Failed to load %s\n", controllerDbPath);
		}
		
	}

	loadInputSettings(&inputsettings);
	loadKeyBinds(&keybinds);
	loadControllerBinds(&padbinds);

	initSDLControllers();

	if (inputsettings.isPs2Controls) {
		registerInputScriptPatches(1);
		patchPs2Buttons();
	} else if (inputsettings.dropdownEnabled) {
		registerInputScriptPatches(0);
	}
}

uint8_t movieSkipHasBeenPressed = 0;

uint8_t movieKeyboardInput() {
	uint8_t *keyboardState = SDL_GetKeyboardState(NULL);

	if (keyboardState[SDL_SCANCODE_SPACE] || keyboardState[SDL_SCANCODE_ESCAPE] || keyboardState[SDL_SCANCODE_RETURN]) {
		if (!movieSkipHasBeenPressed) {
			movieSkipHasBeenPressed = 1;
			return 0;
		}
	} else {
		movieSkipHasBeenPressed = 0;
	}

	return 1;
}

#define spine_buttons_asm(SUCCESS, FAIL) __asm {	\
	__asm push eax	\
	__asm push ebx	\
	__asm mov ebx, comp	\
	__asm mov al, byte ptr [ebx + 0x834]	/* R2 */	\
	__asm test al, al	\
	__asm jne success	\
	__asm mov al, byte ptr [ebx + 0x87c]	/* L2 */	\
	__asm test al, al	\
	__asm jne success	\
	\
__asm failure:	\
	__asm pop ebx	\
	__asm pop eax	\
	__asm mov esp, ebp	\
	__asm pop ebp	\
	__asm push FAIL	\
	__asm ret 0x08	\
	\
__asm success:	\
	__asm pop ebx	\
	__asm pop eax	\
	__asm mov esp, ebp	\
	__asm pop ebp	\
	__asm push SUCCESS	\
	__asm ret 0x08	\
}

#define not_spine_buttons_asm(SUCCESS, FAIL) spine_buttons_asm(FAIL, SUCCESS)

void __stdcall ground_gone(void *comp) {
	not_spine_buttons_asm(0x004bc00a, 0x004bc011);
}

void __stdcall maybe_break_vert_1(void *comp) {
	not_spine_buttons_asm(0x004ba399, 0x004b9428);
}

void __stdcall maybe_break_vert_2(void *comp) {
	not_spine_buttons_asm(0x004ba1d7, 0x004b9440);
}

void __stdcall in_air_physics_recovery(void *comp) {
	spine_buttons_asm(0x004c0c66, 0x004c0c6d);
}

void __stdcall in_air_physics_2(void *comp) {
	spine_buttons_asm(0x004c127b, 0x004c12a8);
}

void __stdcall lip_side_hop(void *comp) {
	spine_buttons_asm(0x004d1283, 0x004d12e8);
}

void patchPs2Buttons() {
	// ps2 controls - fix spine buttons
	patchByte((void *)(0x004bbff6), 0x56);	// PUSH ESI
	patchCall((void *)(0x004bbff6 + 1), ground_gone);
	
	patchByte((void *)(0x004b9410), 0x56);	// PUSH ESI
	patchCall((void *)(0x004b9410 + 1), maybe_break_vert_1);

	patchByte((void *)(0x004b9428), 0x56);	// PUSH ESI
	patchCall((void *)(0x004b9428 + 1), maybe_break_vert_2);
	
	patchByte((void *)(0x004c0c52), 0x56);	// PUSH ESI
	patchCall((void *)(0x004c0c52 + 1), in_air_physics_recovery);
	
	patchByte((void *)(0x004c1267), 0x56);	// PUSH ESI
	patchCall((void *)(0x004c1267 + 1), in_air_physics_2);

	patchByte((void *)(0x004d126f), 0x56);	// PUSH ESI
	patchCall((void *)(0x004d126f + 1), lip_side_hop);
}

void patchInput() {
	// patch SIO::Device
	// process
	patchThisToCdecl((void *)0x005409b0, &processController);
	patchByte((void *)(0x005409b0 + 7), 0xC2);
	patchByte((void *)(0x005409b0 + 8), 0x04);
	patchByte((void *)(0x005409b0 + 9), 0x00);

	patchCall(0x00542090, set_actuators);
	patchByte(0x00542090, 0xe9);	// patch CALL to JMP

	// init input patch - nop direct input setup
	//patchNop(0x005430fc, 49);
	patchNop(0x0054310A, 35);	// DirectInput8Create
	patchNop(0x0054312d, 8);	// createDevice
	patchNop(0x00543147, 12);	// createDevice
	patchNop(0x00543153, 11);	// sleep
	patchCall(0x0054310A + 5, &initManager);

	// patch keyboard input for movies
	patchNop(0x0042a573, 97);
	patchCall(0x0042a573, movieKeyboardInput);
	// TEST AL, AL
	patchByte(0x0042a573 + 5, 0x84);
	patchByte(0x0042a573 + 6, 0xc0);

	// handle events while unfocused
	patchCall(0x0042921f, processEventsUnfocused);
	patchCall(0x0042a612, processEventsUnfocused);

	patchCall(0x00541fe0, activate_actuators);
	patchByte(0x00541fe0, 0xe9);	// patch CALL to JMP
}