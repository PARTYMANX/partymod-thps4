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

#define VERSION_NUMBER_MAJOR 1
#define VERSION_NUMBER_MINOR 0
#define VERSION_NUMBER_FIX 4

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
	printf("TEST: %s\n", masterServerStr);

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
	uint64_t frameTarget = timerFreq / 60;
	//printf("FREQUENCY: %lld, %lld\n", timerFreq, frameTarget);

	if (!nextFrame || nextFrame < SDL_GetPerformanceCounter()) {
		//printf("missed frame target!!\n");
		nextFrame = SDL_GetPerformanceCounter() + frameTarget;
	} else {
		safeWait(nextFrame);
		nextFrame += frameTarget;
	}
}

void patchFrameCap() {
	patchNop(0x004292a0, 58);	// patch out original, too high framerate cap
	patchCall(0x004292a0, do_frame_cap);
}

struct flashVertex {
	float x, y, z, w;
	uint32_t color;
	float u, v;
};

// vertices are passed into the render function in the wrong order when drawing screen flashes; reorder them before passing to draw
void __fastcall reorder_flash_vertices(uint32_t *d3dDevice, void *unused, void *alsodevice, uint32_t prim, uint32_t count, struct flashVertex *vertices, uint32_t stride){
	void(__fastcall *drawPrimitiveUP)(void *, void *, void *, uint32_t, uint32_t, struct flashVertex *, uint32_t) = d3dDevice[72];

	struct flashVertex tmp;

	tmp = vertices[0];
	vertices[0] = vertices[1];
	vertices[1] = vertices[2];
	vertices[2] = tmp;

	drawPrimitiveUP(d3dDevice, unused, alsodevice, prim, count, vertices, stride);
}

void patchScreenFlash() {
	patchNop(0x0044bdce, 6);
	patchCall(0x0044bdce, reorder_flash_vertices);
}

void patchIsPs2() {
	patchByte(0x00510e38, 0xeb);
}

void patchDisableGamma() {
	patchByte(0x004398d0, 0xc3);	// return immediately when changing gamma
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

// big, unused patch for rendering stuff, was trying to fix performance but made it worse lol

void *oldresult = NULL;

void *set_blend_mode_wrapper(int mode, int unk) {
	void *(*set_blend_mode)(int, int) = 0x00440de0;
	int *old_mode = 0x00a72aa4;
	int *old_unk = 0x00a72aa8;

	if (*old_mode != mode || *old_unk != unk) {
		oldresult = set_blend_mode(mode, unk);
		//printf("RETURNING NEW: 0x%08x\n", oldresult);
	} else {
		//printf("RETURNING 0\n");
		return 0;
	}

	return oldresult;
}

void patch_blend_mode() {
	patchCall(0x0043b95e, set_blend_mode_wrapper);
	patchCall(0x00441737, set_blend_mode_wrapper);
	patchCall(0x00442da5, set_blend_mode_wrapper);
	patchCall(0x0044546a, set_blend_mode_wrapper);
	patchCall(0x0044aacd, set_blend_mode_wrapper);
	patchCall(0x0044bd90, set_blend_mode_wrapper);
}

uint32_t changecounter = 0;
uint32_t drawcounter = 0;
uint64_t drawprims = 0;

HRESULT(__fastcall *orig_SetRenderState)(void *, void *, void *, uint32_t, uint32_t) = NULL;
HRESULT(__fastcall *orig_GetRenderState)(void *, void *, void *, uint32_t, uint32_t *) = NULL;
HRESULT(__fastcall *orig_SetTextureStageState)(void *, void *, void *, uint32_t, uint32_t, uint32_t) = NULL;
HRESULT(__fastcall *orig_DrawIndexedPrimitive)(void *, void *, void *, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) = NULL;
HRESULT(__fastcall *orig_DrawIndexedPrimitiveUP)(void *, void *, void *, uint32_t, uint32_t, uint32_t, void *, uint32_t, void *, uint32_t) = NULL;
HRESULT(__fastcall *orig_DrawPrimitive)(void *, void *, void *, uint32_t, uint32_t, uint32_t) = NULL;
HRESULT(__fastcall *orig_DrawPrimitiveUP)(void *, void *, void *, uint32_t, uint32_t, void *, uint32_t) = NULL;
HRESULT(__fastcall *orig_Present)(void *, void *, void *, void *, void *, void *, void *) = NULL;
HRESULT(__fastcall *orig_BeginScene)(void *, void *, void *) = NULL;
HRESULT(__fastcall *orig_SetStreamSource)(void *, void *, void *, uint32_t, void *, uint32_t) = NULL;

uint8_t statechecklist[256];
uint32_t currentstate[256];
uint32_t prevstate[256];

void flush_state_cache(void *device, void *padding, void *alsodevice) {
	for (int i = 0; i < 256; i++) {
		if (statechecklist[i] && prevstate[i] != currentstate[i]) {
			orig_SetRenderState(device, padding, alsodevice, i, currentstate[i]);
			prevstate[i] = currentstate[i];
			changecounter++;
		}
	}
}

void restore_state(void *device, void *padding, void *alsodevice) {
	for (int i = 0; i < 256; i++) {
		if (statechecklist[i]) {
			orig_SetRenderState(device, padding, alsodevice, i, currentstate[i]);
			prevstate[i] = currentstate[i];
			changecounter++;
		}
	}
}

HRESULT __fastcall SetRenderStateWrapper(uint8_t *device, void *padding, void *alsodevice, uint32_t state, uint32_t value) {
	//printf("Setting state 0x%08x to 0x%08x!\n", state, value);
	

	if (state < 255 && !statechecklist[state]) {
		printf("Setting state %d to 0x%08x!\n", state, value);
		statechecklist[state] = 1;
		orig_SetRenderState(device, padding, alsodevice, state, value);
		return D3D_OK;
	} else if (state > 255) {
		printf("!!!!! USED STATE 0x%08x !!!!!\n", state);
		orig_SetRenderState(device, padding, alsodevice, state, value);
		return D3D_OK;
	}

	currentstate[state] = value;

	//orig_SetRenderState(device, padding, alsodevice, state, value);

	return D3D_OK;	// hacky, but it's a released game so they're not going to need to check this
}

HRESULT __fastcall GetRenderStateWrapper(uint8_t *device, void *padding, void *alsodevice, uint32_t state, uint32_t *value) {
	if (state < 255 && statechecklist[state]) {
		*value = currentstate[state];

		return D3D_OK;
	}

	orig_GetRenderState(device, padding, alsodevice, state, value);

	return D3D_OK;	// hacky, but it's a released game so they're not going to need to check this
}

HRESULT __fastcall SetTextureStageStateWrapper(uint8_t *device, void *padding, void *alsodevice, uint32_t stage, uint32_t type, uint32_t value) {
	//printf("Setting texture stage 0x%08x state 0x%08x to 0x%08x!\n", stage, type, value);
	changecounter++;

	if (stage != 0) {
		printf("NON 0 STAGE!!!! %d\n", stage);
	}

	//orig_SetTextureStageState(device, padding, alsodevice, stage, type, value);

	return D3D_OK;	// hacky, but it's a released game so they're not going to need to check this
}

HRESULT __fastcall PresentWrapper(uint8_t *device, void *padding, void *alsodevice, void *a , void *b, void *c, void *d) {
	// haha fun name
	//restore_state(device, padding, alsodevice);

	//printf("%d state changes in frame!\n", changecounter);
	printf("%d draw calls in frame!\n", drawcounter);
	changecounter = 0;
	drawcounter = 0;
	drawprims = 0;

	orig_Present(device, padding, alsodevice, a, b, c, d);

	return D3D_OK;	// hacky, but it's a released game so they're not going to need to check this
}

HRESULT __fastcall BeginSceneWrapper(void *device, void *padding, void *alsodevice) {
	orig_BeginScene(device, padding, alsodevice);

	restore_state(device, padding, alsodevice);	// when focus is lost, state can get clobbered (possibly for other reasons too?) restore at the start of a frame to avoid that

	return D3D_OK;
}

HRESULT __fastcall DrawIndexedPrimitiveWrapper(void *device, void *padding, void *alsodevice, uint32_t prim_type, uint32_t base_idx, uint32_t num_vertices, uint32_t start_idx, uint32_t prim_count) {
	//flush_state_cache(device, padding, alsodevice);
	drawcounter++;
	drawprims += prim_count;

	orig_DrawIndexedPrimitive(device, padding, alsodevice, prim_type, base_idx, num_vertices, start_idx, prim_count);

	return D3D_OK;	// hacky, but it's a released game so they're not going to need to check this
}

//0043d6c5
uint8_t cached_draws = 0;

HRESULT __fastcall DrawIndexedPrimitive_Combine(void *device, void *padding, void *alsodevice, uint32_t prim_type, uint32_t base_idx, uint32_t num_vertices, uint32_t start_idx, uint32_t prim_count) {
	//flush_state_cache(device, padding, alsodevice);
	//drawcounter++;

	cached_draws = 1;

	// store draw info here

	//orig_DrawIndexedPrimitive(device, padding, alsodevice, prim_type, base_idx, num_vertices, start_idx, prim_count);

	return D3D_OK;	// hacky, but it's a released game so they're not going to need to check this
}

HRESULT __fastcall DrawIndexedPrimitiveUPWrapper(void *device, void *padding, void *alsodevice, uint32_t a, uint32_t b, uint32_t c, void *d, uint32_t e, void *f, uint32_t g) {
	//flush_state_cache(device, padding, alsodevice);

	orig_DrawIndexedPrimitiveUP(device, padding, alsodevice, a, b, c, d, e, f, g);

	return D3D_OK;	// hacky, but it's a released game so they're not going to need to check this
}

HRESULT __fastcall DrawPrimitiveWrapper(void *device, void *padding, void *alsodevice, uint32_t a, uint32_t b, uint32_t c) {
	//flush_state_cache(device, padding, alsodevice);

	orig_DrawPrimitive(device, padding, alsodevice, a, b, c);

	return D3D_OK;	// hacky, but it's a released game so they're not going to need to check this
}

HRESULT __fastcall DrawPrimitiveUPWrapper(void *device, void *padding, void *alsodevice, uint32_t a, uint32_t b, void *c, uint32_t d) {
	//flush_state_cache(device, padding, alsodevice);

	orig_DrawPrimitiveUP(device, padding, alsodevice, a, b, c, d);

	return D3D_OK;	// hacky, but it's a released game so they're not going to need to check this
}

HRESULT __fastcall SetStreamSourceWrapper(void *device, void *padding, void *alsodevice, uint32_t streamnumber, void *streamdata, uint32_t stride) {
	printf("%d draw calls %d prims for stream! %08x\n", drawcounter, drawprims, streamdata);
	drawcounter = 0;
	drawprims = 0;

	orig_SetStreamSource(device, padding, alsodevice, streamnumber, streamdata, stride);

	return D3D_OK;
}

void install_device_hooks() {
	// install hooks to perform better state management
	printf("Installing D3D8 Device Hooks\n");

	// get device
	uint8_t *device = **(uint32_t **)0x00aab478;
	uint32_t *d3dDevice = device;

	//orig_SetRenderState = *(uint32_t *)(device + 0xc8);
	orig_SetRenderState = *(uint32_t *)(device + 0xc8);
	orig_GetRenderState = *(uint32_t *)(device + 0xcc);
	orig_SetTextureStageState = *(uint32_t *)(device + 0xf8);
	orig_DrawIndexedPrimitive = *(uint32_t *)(device + 0x11c);
	orig_DrawIndexedPrimitiveUP = *(uint32_t *)(device + 0x124);
	orig_DrawPrimitive = *(uint32_t *)(device + 0x118);
	orig_DrawPrimitiveUP = *(uint32_t *)(device + 0x120);
	orig_Present = *(uint32_t *)(device + 0x3c);
	orig_BeginScene = *(uint32_t *)(device + 0x88);
	orig_SetStreamSource = *(uint32_t *)(device + 0x14c);

	patchDWord(device + 0xc8, SetRenderStateWrapper);
	patchDWord(device + 0xcc, GetRenderStateWrapper);
	patchDWord(device + 0xf8, SetTextureStageStateWrapper);
	patchDWord(device + 0x11c, DrawIndexedPrimitiveWrapper);
	patchDWord(device + 0x124, DrawIndexedPrimitiveUPWrapper);
	patchDWord(device + 0x118, DrawPrimitiveWrapper);
	patchDWord(device + 0x120, DrawPrimitiveUPWrapper);
	patchDWord(device + 0x3c, PresentWrapper);
	patchDWord(device + 0x88, BeginSceneWrapper);
	patchDWord(device + 0x14c, SetStreamSourceWrapper);

	for (int i = 0; i < 256; i++) {
		statechecklist[i] = 0;
		currentstate[i] = -1;
		prevstate[i] = -1;

	}
}

void patch_d3d8_device() {
	// quick warning: you must install the config patches first as this calls a function where ShowWindow() has been nop'd out

	patchCall(0x0043f392, install_device_hooks);
}

void *origCreateDevice = NULL;

int __fastcall createDeviceWrapper(void *id3d8, void *pad, void *id3d8again, uint32_t adapter, uint32_t type, void *hwnd, uint32_t behaviorFlags, uint32_t *presentParams, void *devOut) {
	int (__fastcall *createDevice)(void *, void *, void *, uint32_t, uint32_t, void *, uint32_t, uint32_t *, void *) = (void *)origCreateDevice;

	presentParams[3] = 1;
	presentParams[5] = 1;
	
	//printf("TEST: %d\n", presentParams[8]);

	
	//if (presentParams[7]) {	// if windowed
		presentParams[11] = 0;	// refresh rate
		presentParams[12] = 0;	// swap interval
	//} else {
	//	presentParams[11] = 120;
	//	presentParams[12] = 1;
	//}
	

	int result = createDevice(id3d8, pad, id3d8again, adapter, type, hwnd, behaviorFlags, presentParams, devOut);
	printf("Created modified d3d device\n");

	//if (result == 0 && devOut) {
		//uint8_t *device = **(uint32_t **)devOut;

		//origPresent = *(uint32_t *)(device + 0x3c);
		//patchDWord(device + 0x3c, presentWrapper);
	//}

	return result;
}

void * __stdcall createD3D8Wrapper(uint32_t version) {
	void *(__stdcall *created3d8)(uint32_t) = (void *)0x00546120;

	void *result = created3d8(version);

	if (result) {
		uint8_t *iface = *(uint32_t *)result;

		origCreateDevice = *(uint32_t *)(iface + 0x3c);
		patchDWord(iface + 0x3c, createDeviceWrapper);
	}
	
	return result;
}

void patchRenderer() {
	// remove a big chunk of rendering.  for fun.
	//patchNop(0x0043dbc2, 37);	// dynamic stuff?  skinned meshes?
	//patchNop(0x0043d6a6, 37);	// world

	//patchNop(0x0043d455, 107 + 6);	// remove world mesh state changes

	//patchNop(0x0043d65b, 60);

	//patchNop(0x0043d633, 6);

	//patchNop(0x0043bb15, 55);

	//patchNop(0x0043d380, 5);	// call at start of mesh::submit
	//patchNop(0x0043daea, 33);
	//patchNop(0x0043db10, 3);

	//patchByte(0x00440d70, 0xc3);	// disable texture setting
	//patchByte(0x00440da4, 0xeb);	// disable texture setting

	//patchNop(0x0043d453, 115);	// tons of state changes in mesh::submit

	//patch_blend_mode();

	//patch_d3d8_device();

	patchByte(0x0043f059 + 1, 0x40);	// disable D3D multithreading
	patchByte(0x0043f11f + 1, 0x40);	// disable D3D multithreading
	patchByte(0x0043f170 + 1, 0x20);	// disable D3D multithreading
	patchByte(0x0043f1b2 + 1, 0x40);	// disable D3D multithreading
	patchByte(0x0043f1de + 1, 0x20);	// disable D3D multithreading

	//patchCall(0x0043efbc, createD3D8Wrapper);
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
