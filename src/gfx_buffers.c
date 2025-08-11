#include <d3d.h>
#include <d3dtypes.h>

#include <stdint.h>

#include <patch.h>
#include <config.h>

// this file is poorly named!  check the bottom for actually relevant stuff

// big, unused patch for rendering stuff, was trying to fix performance but made it worse lol

void* oldresult = NULL;

void* set_blend_mode_wrapper(int mode, int unk) {
	void* (*set_blend_mode)(int, int) = 0x00440de0;
	int* old_mode = 0x00a72aa4;
	int* old_unk = 0x00a72aa8;

	if (*old_mode != mode || *old_unk != unk) {
		oldresult = set_blend_mode(mode, unk);
		//printf("RETURNING NEW: 0x%08x\n", oldresult);
	}
	else {
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

HRESULT(__fastcall* orig_SetRenderState)(void*, void*, void*, uint32_t, uint32_t) = NULL;
HRESULT(__fastcall* orig_GetRenderState)(void*, void*, void*, uint32_t, uint32_t*) = NULL;
HRESULT(__fastcall* orig_SetTextureStageState)(void*, void*, void*, uint32_t, uint32_t, uint32_t) = NULL;
HRESULT(__fastcall* orig_DrawIndexedPrimitive)(void*, void*, void*, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) = NULL;
HRESULT(__fastcall* orig_DrawIndexedPrimitiveUP)(void*, void*, void*, uint32_t, uint32_t, uint32_t, void*, uint32_t, void*, uint32_t) = NULL;
HRESULT(__fastcall* orig_DrawPrimitive)(void*, void*, void*, uint32_t, uint32_t, uint32_t) = NULL;
HRESULT(__fastcall* orig_DrawPrimitiveUP)(void*, void*, void*, uint32_t, uint32_t, void*, uint32_t) = NULL;
HRESULT(__fastcall* orig_Present)(void*, void*, void*, void*, void*, void*, void*) = NULL;
HRESULT(__fastcall* orig_BeginScene)(void*, void*, void*) = NULL;
HRESULT(__fastcall* orig_SetStreamSource)(void*, void*, void*, uint32_t, void*, uint32_t) = NULL;
HRESULT(__fastcall* orig_GetStreamSource)(void*, void*, void*, uint32_t, void**, uint32_t) = NULL;
HRESULT(__fastcall* orig_SetIndices)(void*, void*, void*, void **, uint32_t) = NULL;
HRESULT(__fastcall* orig_GetIndices)(void*, void*, void*, void **, uint32_t) = NULL;
HRESULT(__fastcall* orig_CreateVertexBuffer)(void*, void*, void*, uint32_t, uint32_t, uint32_t, uint32_t, void **) = NULL;
HRESULT(__fastcall* orig_CreateIndexBuffer)(void*, void*, void*, uint32_t, uint32_t, uint32_t, uint32_t, void **) = NULL;

uint8_t statechecklist[256];
uint32_t currentstate[256];
uint32_t prevstate[256];

void flush_state_cache(void* device, void* padding, void* alsodevice) {
	for (int i = 0; i < 256; i++) {
		if (statechecklist[i] && prevstate[i] != currentstate[i]) {
			orig_SetRenderState(device, padding, alsodevice, i, currentstate[i]);
			prevstate[i] = currentstate[i];
			changecounter++;
		}
	}
}

void restore_state(void* device, void* padding, void* alsodevice) {
	for (int i = 0; i < 256; i++) {
		if (statechecklist[i]) {
			orig_SetRenderState(device, padding, alsodevice, i, currentstate[i]);
			prevstate[i] = currentstate[i];
			changecounter++;
		}
	}
}

HRESULT __fastcall SetRenderStateWrapper(uint8_t* device, void* padding, void* alsodevice, uint32_t state, uint32_t value) {
	//printf("Setting state 0x%08x to 0x%08x!\n", state, value);


	if (state < 255 && !statechecklist[state]) {
		printf("Setting state %d to 0x%08x!\n", state, value);
		statechecklist[state] = 1;
		orig_SetRenderState(device, padding, alsodevice, state, value);
		return D3D_OK;
	}
	else if (state > 255) {
		printf("!!!!! USED STATE 0x%08x !!!!!\n", state);
		orig_SetRenderState(device, padding, alsodevice, state, value);
		return D3D_OK;
	}

	currentstate[state] = value;

	//orig_SetRenderState(device, padding, alsodevice, state, value);

	return D3D_OK;	// hacky, but it's a released game so they're not going to need to check this
}

HRESULT __fastcall GetRenderStateWrapper(uint8_t* device, void* padding, void* alsodevice, uint32_t state, uint32_t* value) {
	if (state < 255 && statechecklist[state]) {
		*value = currentstate[state];

		return D3D_OK;
	}

	orig_GetRenderState(device, padding, alsodevice, state, value);

	return D3D_OK;	// hacky, but it's a released game so they're not going to need to check this
}

HRESULT __fastcall SetTextureStageStateWrapper(uint8_t* device, void* padding, void* alsodevice, uint32_t stage, uint32_t type, uint32_t value) {
	//printf("Setting texture stage 0x%08x state 0x%08x to 0x%08x!\n", stage, type, value);
	changecounter++;

	if (stage != 0) {
		printf("NON 0 STAGE!!!! %d\n", stage);
	}

	//orig_SetTextureStageState(device, padding, alsodevice, stage, type, value);

	return D3D_OK;	// hacky, but it's a released game so they're not going to need to check this
}

HRESULT __fastcall PresentWrapper(uint8_t* device, void* padding, void* alsodevice, void* a, void* b, void* c, void* d) {
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

HRESULT __fastcall BeginSceneWrapper(void* device, void* padding, void* alsodevice) {
	orig_BeginScene(device, padding, alsodevice);

	restore_state(device, padding, alsodevice);	// when focus is lost, state can get clobbered (possibly for other reasons too?) restore at the start of a frame to avoid that

	return D3D_OK;
}

HRESULT __fastcall DrawIndexedPrimitiveWrapper(void* device, void* padding, void* alsodevice, uint32_t prim_type, uint32_t base_idx, uint32_t num_vertices, uint32_t start_idx, uint32_t prim_count) {
	//flush_state_cache(device, padding, alsodevice);
	drawcounter++;
	drawprims += prim_count;

	orig_DrawIndexedPrimitive(device, padding, alsodevice, prim_type, base_idx, num_vertices, start_idx, prim_count);

	return D3D_OK;	// hacky, but it's a released game so they're not going to need to check this
}

//0043d6c5
uint8_t cached_draws = 0;

HRESULT __fastcall DrawIndexedPrimitive_Combine(void* device, void* padding, void* alsodevice, uint32_t prim_type, uint32_t base_idx, uint32_t num_vertices, uint32_t start_idx, uint32_t prim_count) {
	//flush_state_cache(device, padding, alsodevice);
	//drawcounter++;

	cached_draws = 1;

	// store draw info here

	//orig_DrawIndexedPrimitive(device, padding, alsodevice, prim_type, base_idx, num_vertices, start_idx, prim_count);

	return D3D_OK;	// hacky, but it's a released game so they're not going to need to check this
}

HRESULT __fastcall DrawIndexedPrimitiveUPWrapper(void* device, void* padding, void* alsodevice, uint32_t a, uint32_t b, uint32_t c, void* d, uint32_t e, void* f, uint32_t g) {
	//flush_state_cache(device, padding, alsodevice);

	orig_DrawIndexedPrimitiveUP(device, padding, alsodevice, a, b, c, d, e, f, g);

	return D3D_OK;	// hacky, but it's a released game so they're not going to need to check this
}

HRESULT __fastcall DrawPrimitiveWrapper(void* device, void* padding, void* alsodevice, uint32_t a, uint32_t b, uint32_t c) {
	//flush_state_cache(device, padding, alsodevice);

	orig_DrawPrimitive(device, padding, alsodevice, a, b, c);

	return D3D_OK;	// hacky, but it's a released game so they're not going to need to check this
}

HRESULT __fastcall DrawPrimitiveUPWrapper(void* device, void* padding, void* alsodevice, uint32_t a, uint32_t b, void* c, uint32_t d) {
	//flush_state_cache(device, padding, alsodevice);

	orig_DrawPrimitiveUP(device, padding, alsodevice, a, b, c, d);

	return D3D_OK;	// hacky, but it's a released game so they're not going to need to check this
}

HRESULT __fastcall SetStreamSourceWrapper(void* device, void* padding, void* alsodevice, uint32_t streamnumber, void* streamdata, uint32_t stride) {
	printf("%d draw calls %d prims for stream! %08x\n", drawcounter, drawprims, streamdata);
	drawcounter = 0;
	drawprims = 0;

	orig_SetStreamSource(device, padding, alsodevice, streamnumber, streamdata, stride);

	return D3D_OK;
}

HRESULT __fastcall CreateVertexBufferWrapper(void* device, void* padding, void* alsodevice, uint32_t length, uint32_t usage, uint32_t fvf, uint32_t pool, void** ppResult) {
	printf("CREATING VERTEX BUFFER: LEN: %d, USAGE: 0x%08x, FVF: 0x%08x, POOL: 0x%08x\n", length, usage, fvf, pool);

	//usage &= 0x208;
	// idea: if managed and fvf = 0, allocate in big buffer

	return orig_CreateVertexBuffer(device, padding, alsodevice, length, usage, fvf, pool, ppResult);
}

HRESULT __fastcall CreateIndexBufferWrapper(void* device, void* padding, void* alsodevice, uint32_t length, uint32_t usage, uint32_t format, uint32_t pool, void** ppResult) {
	printf("CREATING INDEX BUFFER: LEN: %d, USAGE: 0x%08x, FORMAT: 0x%08x, POOL: 0x%08x\n", length, usage, format, pool);

	//usage &= 0x208;
	pool = 0x01;

	return orig_CreateIndexBuffer(device, padding, alsodevice, length, usage, format, pool, ppResult);
}

void install_device_hooks(uint8_t* in_device) {
	// install hooks to perform better state management
	printf("Installing D3D8 Device Hooks\n");

	// get device
	uint8_t* device = *(uint32_t*)in_device;
	uint32_t* d3dDevice = device;

	//orig_SetRenderState = *(uint32_t *)(device + 0xc8);
	/*orig_SetRenderState = *(uint32_t*)(device + 0xc8);
	orig_GetRenderState = *(uint32_t*)(device + 0xcc);
	orig_SetTextureStageState = *(uint32_t*)(device + 0xf8);
	orig_DrawIndexedPrimitive = *(uint32_t*)(device + 0x11c);
	orig_DrawIndexedPrimitiveUP = *(uint32_t*)(device + 0x124);
	orig_DrawPrimitive = *(uint32_t*)(device + 0x118);
	orig_DrawPrimitiveUP = *(uint32_t*)(device + 0x120);
	orig_Present = *(uint32_t*)(device + 0x3c);
	orig_BeginScene = *(uint32_t*)(device + 0x88);
	orig_SetStreamSource = *(uint32_t*)(device + 0x14c);*/



	/*patchDWord(device + 0xc8, SetRenderStateWrapper);
	patchDWord(device + 0xcc, GetRenderStateWrapper);
	patchDWord(device + 0xf8, SetTextureStageStateWrapper);
	patchDWord(device + 0x11c, DrawIndexedPrimitiveWrapper);
	patchDWord(device + 0x124, DrawIndexedPrimitiveUPWrapper);
	patchDWord(device + 0x118, DrawPrimitiveWrapper);
	patchDWord(device + 0x120, DrawPrimitiveUPWrapper);
	patchDWord(device + 0x3c, PresentWrapper);
	patchDWord(device + 0x88, BeginSceneWrapper);
	patchDWord(device + 0x14c, SetStreamSourceWrapper);*/

	for (int i = 0; i < 256; i++) {
		statechecklist[i] = 0;
		currentstate[i] = -1;
		prevstate[i] = -1;

	}

	orig_DrawPrimitive = *(uint32_t*)(device + 0x118);
	orig_DrawPrimitiveUP = *(uint32_t*)(device + 0x120);
	orig_DrawIndexedPrimitive = *(uint32_t*)(device + 0x11c);
	orig_DrawIndexedPrimitiveUP = *(uint32_t*)(device + 0x124);
	orig_CreateVertexBuffer = *(uint32_t*)(device + 0x5c);
	orig_CreateIndexBuffer = *(uint32_t*)(device + 0x60);
	orig_SetStreamSource = *(uint32_t*)(device + 0x14c);
	orig_GetStreamSource = *(uint32_t*)(device + 0x150);
	orig_SetIndices = *(uint32_t*)(device + 0x154);
	orig_GetIndices = *(uint32_t*)(device + 0x158);

	printf("INSTALLED CREATE VERTEX BUFFER WRAPPER\n");

	patchDWord(device + 0x5c, CreateVertexBufferWrapper);
	patchDWord(device + 0x60, CreateIndexBufferWrapper);
	//patchDWord(device + 0x14c, SetStreamSourceWrapper);
}

void patch_d3d8_device() {
	// quick warning: you must install the config patches first as this calls a function where ShowWindow() has been nop'd out

	patchCall(0x0043f392, install_device_hooks);
}

void* origCreateDevice = NULL;

int config_vsync_mode = 0;

// wrapper for createDevice that selects the optimal mode for vsync
int __fastcall createDeviceWrapper(void* id3d8, void* pad, void* id3d8again, uint32_t adapter, uint32_t type, void* hwnd, uint32_t behaviorFlags, uint32_t* presentParams, void** devOut) {
	int(__fastcall * createDevice)(void*, void*, void*, uint32_t, uint32_t, void*, uint32_t, uint32_t*, void*) = (void*)origCreateDevice;

	presentParams[3] = 1;
	presentParams[5] = 1;

	//printf("TEST: %d\n", presentParams[8]);


	if (presentParams[7]) {	// if windowed
		presentParams[11] = 0;	// refresh rate
		presentParams[12] = 0;	// swap interval
	}
	else {
		presentParams[11] = 0;
		presentParams[12] = 1;

		/*uint32_t freq = 0;
		uint32_t interval = 0;

		if (config_vsync_mode == 2) {
			getOptimalRefreshRate(&freq, &interval);

			// TODO: test unsupported resolutions
			presentParams[11] = freq;
			presentParams[12] = interval;

			printf("REFRESH: %d, SWAP INTERVAL: %d\n", presentParams[11], presentParams[12]);
		} else if (config_vsync_mode == 1) {
			presentParams[11] = 0;
			presentParams[12] = 1;
		} else {
			presentParams[11] = 0;
			presentParams[12] = 0;
		}*/
	}


	int result = createDevice(id3d8, pad, id3d8again, adapter, type, hwnd, behaviorFlags, presentParams, devOut);
	//printf("Created modified d3d device\n");

	//if (result == 0 && devOut) {
		//uint8_t *device = **(uint32_t **)devOut;

		//origPresent = *(uint32_t *)(device + 0x3c);
		//patchDWord(device + 0x3c, presentWrapper);
	//}

	//install_device_hooks(*devOut);

	return result;
}

void* __stdcall createD3D8Wrapper(uint32_t version) {
	void* (__stdcall * created3d8)(uint32_t) = (void*)0x00546120;

	void* result = created3d8(version);

	if (result) {
		uint8_t* iface = *(uint32_t*)result;

		origCreateDevice = *(uint32_t*)(iface + 0x3c);
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
}

void patchVSync() {
	patchCall(0x0043efbc, createD3D8Wrapper);
}
