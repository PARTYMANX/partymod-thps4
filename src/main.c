#include <windows.h>
#include <d3d9.h>

#include <stdio.h>
#include <stdint.h>
#include <time.h>

#include <SDL2/SDL.h>

#include <patch.h>
#include <global.h>
#include <input.h>
#include <config.h>
#include <script.h>
#include <gfx.h>

#define VERSION_NUMBER_MAJOR 1
#define VERSION_NUMBER_MINOR 0
#define VERSION_NUMBER_FIX 9

// FIXME: still broken, not sure why
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

void patchDisableGamma();
void patchFrameCap();

uint32_t rng_seed = 0;


char domainStr[256];
char masterServerStr[266];

void patchOnlineService(char *configFile) {
	GetPrivateProfileString("Miscellaneous", "OnlineDomain", "openspy.net", domainStr, 256, configFile);

	sprintf(masterServerStr, "%%s.master.%s", domainStr);
	//printf("TEST: %s\n", masterServerStr);

	patchDWord(0x00544a1c + 1, masterServerStr);

	printf("Patched online server: %s\n", domainStr);
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
	printf("PARTYMOD for THPS4 %d.%d.%d\n", VERSION_NUMBER_MAJOR, VERSION_NUMBER_MINOR, VERSION_NUMBER_FIX);

	printf("DIRECTORY: %s\n", executableDirectory);

	//patchResolution();

	initScriptPatches();

	/*int disableMovies = getIniBool("Miscellaneous", "NoMovie", 0, configFile);
	if (disableMovies) {
		printf("Disabling movies\n");
		patchNoMovie();
	}*/

	int disableGamma = getIniBool("Graphics", "DisableFullscreenGamma", 1, configFile);
	if (disableGamma) {
		patchDisableGamma();
	}

	int disablePhysicsFixes = getIniBool("Miscellaneous", "DisablePhysicsFixes", 0, configFile);
	if (!disablePhysicsFixes) {
		patchLedgeWarp();
		patchFriction();
	}

	int disableFramerateCap = getIniBool("Miscellaneous", "DisableFramerateCap", 0, configFile);
	if (!disableFramerateCap) {
		patchFrameCap();
	}

	int disableGrass = getIniBool("Graphics", "DisableGrassEffect", 0, configFile);
	if (disableGrass) {
		patchNoGrass();
	}

	patchOnlineService(configFile);

	// get some source of entropy for the music randomizer
	rng_seed = time(NULL) & 0xffffffff;
	srand(rng_seed);

	printf("Patch Initialized\n");
}

uint8_t did_logic = 0;
void __fastcall do_system_logic(void *stack, void *padding, uint8_t is_profiling) {
	void (__fastcall *process_tasks)(void *, void *, uint8_t) = (void *)0x00406670;

	if (!did_logic) {
		process_tasks(stack, padding, is_profiling);
	}
}

void __fastcall do_game_logic(void *stack, void *padding, uint8_t is_profiling) {
	void (__fastcall *process_tasks)(void *, void *, uint8_t) = (void *)0x00406670;

	if (!did_logic) {
		process_tasks(stack, padding, is_profiling);
	}
	did_logic = !did_logic;
}

void patchLogicRate() {
	patchCall(0x00429423, do_system_logic);
	patchCall(0x00429437, do_game_logic);
}

void safeWait(uint64_t endTime) {
	uint64_t timerFreq = SDL_GetPerformanceFrequency();
	uint64_t safetyThreshold = timerFreq / 1000 * 3;	// 3ms

	uint64_t currentTime = SDL_GetPerformanceCounter();

	//printf("%f, %d, %f\n", (double)(nextTime - currentTime) / timerFreq, inFrame->best_effort_timestamp, timebase);

	while (currentTime < endTime) {
		currentTime = SDL_GetPerformanceCounter();

		//printf("%f\n", timerAccumulator);

		if (endTime - currentTime > safetyThreshold) {
			SDL_Delay(1);
			//printf("BIG yawn!\n");
		}
	}
	//printf("wait error - %fms - %d\n", ((double)(endTime - currentTime) / (double)timerFreq) / 1000.0, endTime - currentTime);
}

uint64_t nextFrame = 0;

void do_frame_cap() {
	uint64_t timerFreq = SDL_GetPerformanceFrequency();
	uint64_t frameTarget = (timerFreq / 60);
	//printf("FREQUENCY: %lld, %lld\n", timerFreq, frameTarget);

	if (!nextFrame || nextFrame < SDL_GetPerformanceCounter()) {
		//printf("missed frame target!!\n");
		nextFrame = SDL_GetPerformanceCounter() + frameTarget;
	} else {
		safeWait(nextFrame);
		nextFrame += frameTarget;
	}
}

void endframewrapper() {
	void (*endframe)() = 0x00461940;

	do_frame_cap();

	endframe();
}

void patchFrameCap() {
	// put endscene before present
	for (uint8_t *i = 0x0042945d; i < 0x0042946b; i++) {
		patchCopyByte(i - 5, i);
	}
	patchCall(0x00429466, 0x00461940);

	patchNop(0x004292a0, 58);	// patch out original, too high framerate cap
	patchCall(0x004292a0, do_frame_cap);
	//patchCall(0x00429466, endframewrapper);
}

void patchIsPs2() {
	patchByte(0x00510e38, 0xeb);
}

void patchPrintf() {
	// these all seem to crash the game.  oh well!
	//patchCall(0x00535ef0, printf);
	//patchByte(0x00535ef0, 0xe9);	// CALL to JMP
	
	patchCall(0x00405b60, printf);
	patchByte(0x00405b60, 0xe9);	// CALL to JMP
}

int isCD() {
	return 0;
}

int isNotCD() {
	return 1;
}

void patchCD() {
	patchCall(0x00535f00, isNotCD);
	patchByte(0x00535f00, 0xe9);

	patchCall(0x00543fd0, isCD);
	patchByte(0x00543fd0, 0xe9);
}

void our_random(int out_of) {
	// first, call the original random so that we consume a value.  
	// juuust in case someone wants actual 100% identical behavior between partymod and the original game
	void (__cdecl *their_random)(int) = (void *)0x00402c40;

	their_random(out_of);

	return rand() % out_of;
}

void patchRandomMusic() {
	patchCall(0x0042cb74, our_random);
}

void patchOnlineFixes() {
	patchNop(0x00544b1c, 2);
	patchByte(0x0042d4a6, 0xeb);
}

void patchSelectShift() {
	// experimental patch trying to get freecam
	patchNop(0x00505065, 6);
	patchNop(0x00504fef, 2);
}

/*
	graphics patch planning:
	patch vertex buffer stuff in sMesh::Initialize, sMesh::Clone, ~sMesh, and sMesh::Submit
	Create an object based upon the d3d8 buffer that has the same methods, but wraps it with a central buffer
*/

__declspec(dllexport) BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
	// Perform actions based on the reason for calling.
	switch(fdwReason) { 
		case DLL_PROCESS_ATTACH:
			// Initialize once for each new process.
			// Return FALSE to fail DLL load.

			// install patches
			patchWindow();
			patchInput();
			patchCall((void *)(0x005319ab), &(initPatch));
			patchScriptHook();
			patchScreenFlash();
			patchRandomMusic();
			patchOnlineFixes();

			patchRenderer();

			//patchAnisotropicFilter();

			patchUIPositioning();
			patchMovieBlackBars();

			patchVertexBufferCreation();
			
			//patchPrintf();
			//patchCD();

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
