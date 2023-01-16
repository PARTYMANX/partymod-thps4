#include <windows.h>

#include <stdio.h>
#include <stdint.h>

#include <SDL2/SDL.h>

#include <patch.h>
#include <global.h>
#include <input.h>
#include <config.h>
#include <script.h>

#define VERSION_NUMBER_MAJOR 0
#define VERSION_NUMBER_MINOR 1

int __stdcall playIntroMovie(char *filename, int unk) {
	void *(__stdcall *getMoviePlayer)(int) = (void *)0x00406d40;
	void (__stdcall *destroyMoviePlayer)() = (void *)0x00406d90;
	void (__stdcall *loadMovie)(char *) = (void *)0x00407940;
	uint64_t (__stdcall *getCurrentTime)() = (void *)0x00409ae0;
	uint8_t (__fastcall *isMovieFinished)(void *) = (void *)0x004077a0;
	void (__fastcall *endMovie)(void *) = (void *)0x00407620;

	void *moviePlayer = getMoviePlayer(0);
	loadMovie(filename);
	uint64_t startTime = getCurrentTime();
	while (1) {
		if (isMovieFinished(moviePlayer)) {
			break;
		}
		uint64_t currentTime = getCurrentTime();
		uint64_t timeElapsed = currentTime - startTime;
		uint8_t isRolled = currentTime < startTime;
		uint64_t elapsedUnk = (currentTime >> 0x20) - (startTime >> 0x20);

		int isSkip = processIntroEvent();
		if (isSkip == 2) {
			endMovie(moviePlayer);
			destroyMoviePlayer();
			return 0;
		}
		if (unk == 2 && (elapsedUnk != isRolled || 250 < timeElapsed) && isSkip) {	// allow skips after a quarter of a second	(original was 3 seconds)
			break;
		}
	}

	endMovie(moviePlayer);
	destroyMoviePlayer();
	return 1;
}

void patchIntroMovie() {
	patchNop((void *)0x0040a1a0, 0x0040a33f - 0x0040a1a0 + 1);
	patchDWord(0x0040a1a0, 0x0824448b);	// MOV EAX, dword ptr [ESP + 0x08] (push second param back onto stack)
	patchByte(0x0040a1a0 + 4, 0x50);		// PUSH EAX
	patchDWord(0x0040a1a0 + 5, 0x0824448b);	// MOV EAX, dword ptr [ESP + 0x08] (push first param back onto stack)
	patchByte(0x0040a1a0 + 9, 0x50);		// PUSH EAX
	patchCall(0x0040a1a0 + 10, playIntroMovie);
	patchDWord(0x0040a1a0 + 15, 0x0824748b);		// MOV ESI, dword ptr [ESP + 0x08] (move second param into ESI, which future calls expect for some reason)
	patchByte(0x0040a1a0 + 19, 0xc3);	// RET
}

void patchNoMovie() {
	// nop out video playback for compatibility with systems that cannot handle directplay
	patchNop(0x004079a8, 0x004079f4 + 5 - 0x004079a8);
}

/*void ledgeWarpFix() {
	// clamp st(0) to [-1, 1]
	// replaces acos call, so we call that at the end
	__asm {
		ftst
		jl negative
		fld1
		fcom
		fstp st(0)
		jle end
		fstp st(0)
		fld1
		jmp end
	negative:
		fchs
		fld1
		fcom
		fstp st(0)
		fchs
		jle end
		fstp st(0)
		fld1
		fchs
	end:
		
	}

	callFunc(0x00577cd7);
}*/

double ledgeWarpFix(double n) {
	//printf("DOING LEDGE WARP FIX\n");
	//double (__cdecl *orig_acos)(double) = (void *)0x00574ad0;

	__asm {
		sub esp,0x08
		fst qword ptr [esp - 0x08]

		ftst
		jl negative
		fld1
		fcom
		fstp st(0)
		jle end
		fstp st(0)
		fld1
		jmp end
	negative:
		fchs
		fld1
		fcom
		fstp st(0)
		fchs
		jle end
		fstp st(0)
		fld1
		fchs
	end:
		
		add esp,0x08

	}

	callFunc(0x00574ad0);

	//return orig_acos(n);
}

void patchLedgeWarp() {
	patchCall(0x004bc32b, ledgeWarpFix);
}

void __fastcall do_ground_friction(void *skater) {
	//printf("DOING FRICTION FIX\n");

	uint8_t (__fastcall *handle_slope)(void *) = (void *)0x004ba620;
	void (__cdecl *apply_friction)(float *, float, float) = (void *)0x004bb5f0;

	if (!*(int *)(0x0059b680)) {
		*(int *)(0x00aab48c) = 1;
	}

	/*float *vel = *(int *)((int)skater + 0x634) + 0x30;
	float length = sqrtf((vel[0] * vel[0]) + (vel[1] * vel[1]) + (vel[2] * vel[2]));
	float friction = *(float *)((int)skater + 0x37b4);
	float origFriction = *(float *)(*(int *)((int)skater + 0x634) + 0xe0) * 60.0;
	float correctedFriction = *(float *)(*(int *)((int)skater + 0x634) + 0xe0) * *(float *)(*(int *)((int)skater + 0x634) + 0x38) * 60.0;
	float frametime = *(float *)(*(int *)((int)skater + 0x634) + 0x38);
	float unk = *(float *)(*(int *)((int)skater + 0x634) + 0xe0);
	float calcFriction = friction * (1.0 / 60.0) * 60.0 / length;

	printf("ORIG: %f CORRECTED: %f FRAMETIME: %f UNK: %f FRICTION: %f LENGTH: %f CALCFRICTION: %f\n", origFriction, correctedFriction, frametime, unk, friction, length, calcFriction);*/

	if (!handle_slope(skater)) {
		// do the calculation in double to avoid precision issues
		float *vel = *(int *)((int)skater + 0x634) + 0x30;
		double frictionVector[4];
		for (int i = 0; i < 4; i++) {
			frictionVector[i] = vel[i];
		}

		double length = sqrtf((frictionVector[0] * frictionVector[0]) + (frictionVector[1] * frictionVector[1]) + (frictionVector[2] * frictionVector[2]));

		if (length < 0.0001) {
			vel[0] = 0.0;
			vel[1] = 0.0;
			vel[2] = 0.0;
			vel[3] = 0.0;

			return;
		}

		double friction = *(float *)((int)skater + 0x37b4);
		double frametime = *(float *)(*(int *)((int)skater + 0x634) + 0xe0);

		double calcFriction = (friction * frametime * 60.0) / length;

		frictionVector[0] *= calcFriction;
		frictionVector[1] *= calcFriction;
		frictionVector[2] *= calcFriction;
		frictionVector[3] *= calcFriction;

		double frictionD = (frictionVector[0] * frictionVector[0]) + (frictionVector[1] * frictionVector[1]) + (frictionVector[2] * frictionVector[2]);

		if (frictionD > length * length) {
			vel[0] = 0.0;
			vel[1] = 0.0;
			vel[2] = 0.0;
			vel[3] = 0.0;

			return;
		} else {
			double velocityDouble[4];
			for (int i = 0; i < 4; i++) {
				velocityDouble[i] = vel[i];
			}

			velocityDouble[0] -= frictionVector[0];
			velocityDouble[1] -= frictionVector[1];
			velocityDouble[2] -= frictionVector[2];
			velocityDouble[3] -= frictionVector[3];

			vel[0] = velocityDouble[0];
			vel[1] = velocityDouble[1];
			vel[2] = velocityDouble[2];
			vel[3] = velocityDouble[3];
		}
	}
}

void apply_air_friction(void *skater, float friction) {
	float *vel = *(int *)((int)skater + 0x634) + 0x30;

	double velD = (vel[0] * vel[0]) + (vel[1] * vel[1]) + (vel[2] * vel[2]);
	if (velD < 0.00001f) {
		return;
	}

	double frictionVector[4];
	for (int i = 0; i < 4; i++) {
		frictionVector[i] = vel[i];
	}

	double frametime = *(float *)(*(int *)((int)skater + 0x634) + 0xe0);
	double scale = friction * frametime * 60.0 * velD;

	double len = sqrtf((frictionVector[0] * frictionVector[0]) + (frictionVector[1] * frictionVector[1]) + (frictionVector[2] * frictionVector[2]));

	len = scale / len;

	frictionVector[0] *= len;
	frictionVector[1] *= len;
	frictionVector[2] *= len;

	double frictionD = (frictionVector[0] * frictionVector[0]) + (frictionVector[1] * frictionVector[1]) + (frictionVector[2] * frictionVector[2]);
	if (frictionD > velD) {
		vel[0] = 0.0;
		vel[1] = 0.0;
		vel[2] = 0.0;
		vel[3] = 0.0;
	} else {
		double velocityDouble[4];
		for (int i = 0; i < 4; i++) {
			velocityDouble[i] = vel[i];
		}

		velocityDouble[0] -= frictionVector[0];
		velocityDouble[1] -= frictionVector[1];
		velocityDouble[2] -= frictionVector[2];
		velocityDouble[3] -= frictionVector[3];

		vel[0] = velocityDouble[0];
		vel[1] = velocityDouble[1];
		vel[2] = velocityDouble[2];
		vel[3] = velocityDouble[3];
	}
}

float getScriptFloat(uint32_t sum, int def) {
	float (__fastcall *getFloat)(uint32_t, int) = (void *)0x00419610;
	float result;

	__asm {
		push def
		push sum
		call getFloat
		fstp result
	}

	return result;
}

void __fastcall do_air_friction(void *skater) {
	float (__fastcall *getFloat)(uint32_t) = (void *)0x00419610;

	float crouch_friction = getScriptFloat(0xbed96eda, 0);
	float stand_friction = getScriptFloat(0x1a78b6fc, 0);

	float friction_override_time = *(float *)((int)skater + 0x337c);
	if (friction_override_time != 0.0f) {
		crouch_friction = *(float *)((int)skater + 0x3388);	// friction override value
		stand_friction = crouch_friction;
	}

	uint8_t crouching = *(uint8_t *)((int)skater + 0x33dc);
	if (crouching) {
		apply_air_friction(skater, crouch_friction);
	} else {
		apply_air_friction(skater, stand_friction);
	}
}

void __fastcall speed_limiter(void *skater) {
	float (__fastcall *getFloat)(uint32_t) = (void *)0x00419610;
	float friction = getScriptFloat(0x850eb87a, 0);

	apply_air_friction(skater, friction);
}

void get_fake_timer() {
	double time = 1.0 / 240.0;	// 1/60
	__asm {
		fld time
	}
}

void patchTimer() {
	patchCall(0x00543d70, get_fake_timer);
	patchByte(0x00543d70+5, 0xc3);
}

void patchFriction() {
	//patchTimer();
	//patchCall(0x004c9946, get_fake_timer);

	patchCall(0x004c0131, do_air_friction);
	patchCall(0x004c0138, do_ground_friction);

	// speed limiter
	patchNop(0x004bb0df, 174);
	patchByte(0x004bb0df, 0x89);
	patchByte(0x004bb0df + 1, 0xf1);
	patchCall(0x004bb0df + 2, speed_limiter);

	
	//patchNop(0x004c0138, 5);
}

// fixed rendering function for the opaque side of the rendering pipeline
// respects a material flag 0x40 that appears to denote objects that require single sided rendering, while others draw un-culled
void __cdecl fixedDrawWorldAgain(int16_t param_1, int param_2) {
	int32_t *lastVertexBuffer = 0x00906760;
	int32_t *rwObj = 0x00970a8c;
	uint32_t *currentTexture = 0x0090675c;
	int *numMeshes = 0x005c94c8;
	int *meshList = 0x00901758;
	void (__cdecl *RwGetRenderState)(int, void *) = (void *)0x0055ce60;
	void (__cdecl *RwSetRenderState)(int, int) = (void *)0x0055ce10;
	void (__cdecl *SetCurrentTexture)(int **) = (void *)0x004f4320;
	void (__cdecl *SomeLightingStuff)(int, int) = (void *)0x0052fae0;
	void (__cdecl *DrawWorldMesh)(int32_t, int *) = (void *)0x004f4210;

	int32_t uVar1;
	int texture;
	int i;
	int *mesh;	// unknown struct
  
	uVar1 = *rwObj;
	SomeLightingStuff(0,0);
	*lastVertexBuffer = 0xffffffff;
	*currentTexture = -1;
	RwSetRenderState(0x14,2);
	i = 0;
	if (0 < *numMeshes) {
		mesh = meshList;
		texture = *currentTexture;
		while (i < *numMeshes) {
			if ((mesh[2] != 0) && ((*(uint16_t *)(*mesh + 0x1a) & param_1) == 0)) {
				if (texture != *mesh) {
					SetCurrentTexture(mesh);
					if ((*(uint8_t *)(*mesh + 0x1a) & 0x40)) {
						RwSetRenderState(0x14,2);
					} else {
						RwSetRenderState(0x14,1);
					}
				}
				DrawWorldMesh(*rwObj, mesh[3]);
				texture = mesh[0];
				*currentTexture = texture;
			}
			i = i + 1;
			mesh = mesh + 5;
		}
	}
	RwSetRenderState(0x14,2);
	RwSetRenderState(8,1);
	return;
}

void patchCullModeFix() {
	// sets the culling mode to always be NONE rather than BACK
	// fixes missing geometry in a few levels, fixes missing faces in shadows
	
	// transparent stuff
	//patchByte(0x004f4883+1, 0x01);
	// not sure
	//patchByte(0x004f4933+1, 0x01);
	// world?
	//patchByte(0x004f4953+1, 0x01);
	// not sure
	//patchByte(0x004f5c5d+1, 0x03);
	// UI text?
	//patchByte(0x004f5cae+1, 0x01);
	// UI Button Background
	//patchByte(0x004f60d8+1, 0x03);
	// Main UI Text?
	//patchByte(0x004f60f9+1, 0x01);
	// characters?
	//patchByte(0x004f9ca3+1, 0x03);
	// shadow - this doesn't seem like a graceful fix... but it seems to work
	patchByte(0x004f9d95+1, 0x01);

	patchCall(0x004f9ba2, fixedDrawWorldAgain);
	patchCall(0x0042db88, fixedDrawWorldAgain);
}

void initPatch() {
	GetModuleFileName(NULL, &executableDirectory, filePathBufLen);

	// find last slash
	char *exe = strrchr(executableDirectory, '\\');
	if (exe) {
		*(exe + 1) = '\0';
	}

	char configFile[1024];
	sprintf(configFile, "%s%s", executableDirectory, CONFIG_FILE_NAME);

	int isDebug = getIniBool("Miscellaneous", "Debug", 0, configFile);

	if (isDebug) {
		AllocConsole();

		FILE *fDummy;
		freopen_s(&fDummy, "CONIN$", "r", stdin);
		freopen_s(&fDummy, "CONOUT$", "w", stderr);
		freopen_s(&fDummy, "CONOUT$", "w", stdout);
	}
	printf("PARTYMOD for THPS4 %d.%d\n", VERSION_NUMBER_MAJOR, VERSION_NUMBER_MINOR);

	printf("DIRECTORY: %s\n", executableDirectory);

	//patchResolution();

	//initScriptPatches();

	/*int disableMovies = getIniBool("Miscellaneous", "NoMovie", 0, configFile);
	if (disableMovies) {
		printf("Disabling movies\n");
		patchNoMovie();
	}*/

	printf("Patch Initialized\n");
}

void patchNoLauncher() {
	// prevent the setup utility from starting
	// NOTE: if you need some free bytes, there's a lot to work with here, 0x0040b9da to 0x0040ba02 can be rearranged to free up like 36 bytes.  easily enough for a function call
	// the function to load config is in there too, so that can also be taken care of now
	patchNop((void *)0x0040b9da, 7);    // remove call to run launcher
	patchInst((void *)0x0040b9e1, JMP8);    // change launcher condition jump from JZ to JMP
	patchNop((void *)0x0040b9fc, 12);   // remove call to change registry

	// TODO: rename function to make it clear that this adds patch init
	patchCall((void *)0x0040b9da, &(initPatch));
}

void patchNotCD() {
	// Make "notCD" cfunc return true to enable debug features (mostly boring)
	patchByte((void *)0x00404350, 0xb8);
	patchDWord((void *)(0x00404350 + 1), 0x01);
	patchByte((void *)(0x00404350 + 5), 0xc3);
}

__declspec(dllexport) BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
	// Perform actions based on the reason for calling.
	switch(fdwReason) { 
		case DLL_PROCESS_ATTACH:
			// Initialize once for each new process.
			// Return FALSE to fail DLL load.

			// install patches
			//patchNotCD();
			//patchWindow();
			//patchNoLauncher();
			//patchIntroMovie();
			patchLedgeWarp();
			patchFriction();
			//patchCullModeFix();
			patchByte(0x0043f021, 0x1d);	// crappy window hack
			//patchNop(0x0043f037, 6);
			patchInput();
			patchCall((void *)(0x0054310A), &(initPatch));
			//patchLoadConfig();
			//patchScriptHook();

			break;

		case DLL_THREAD_ATTACH:
			// Do thread-specific initialization.
			break;

		case DLL_THREAD_DETACH:
			// Do thread-specific cleanup.
			break;

		case DLL_PROCESS_DETACH:
			// Perform any necessary cleanup.
			break;
	}
	return TRUE;
}
