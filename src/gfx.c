#include <d3d.h>
#include <d3dtypes.h>

#include <stdint.h>

#include <patch.h>

struct flashVertex {
	float x, y, z, w;
	uint32_t color;
	float u, v;
};

// vertices are passed into the render function in the wrong order when drawing screen flashes; reorder them before passing to draw
void __fastcall reorder_flash_vertices(uint32_t* d3dDevice, void* unused, void* alsodevice, uint32_t prim, uint32_t count, struct flashVertex* vertices, uint32_t stride) {
	void(__fastcall * drawPrimitiveUP)(void*, void*, void*, uint32_t, uint32_t, struct flashVertex*, uint32_t) = d3dDevice[72];

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

void patchDisableGamma() {
	patchByte(0x004398d0, 0xc3);	// return immediately when changing gamma
}

void patchNoGrass() {
	patchByte(0x00439b0c, 0xe9);
	patchByte(0x00439b0c + 1, 0xfe);
	patchByte(0x00439b0c + 2, 0x02);
	patchByte(0x00439b0c + 3, 0x00);
	patchByte(0x00439b0c + 4, 0x00);
	patchByte(0x00439b0c + 5, 0x00);
}

HRESULT __fastcall SetTextureStageStateWrapper_SetAnisotropy(void* padding, void* padding2, uint8_t flags, uint32_t stage, uint32_t type, uint32_t value) {
	uint8_t* device = 0x00aab478;
	HRESULT(__fastcall * SetTextureStageState)(void*, void*, void*, uint32_t, uint32_t, uint32_t) = *(uint32_t*)(device + 0xf8);

	HRESULT result = D3D_OK;

	if (flags & 0x80) {
		result = SetTextureStageState(device, padding2, device, stage, type, value);
	}

	return result;
}

void patchAnisotropicFilter() {
	// while drawing
	patchNop(0x0043d54a, 2);

	patchNop(0x0043d54c, 5);
	patchByte(0x0043d54c, 0x8a);
	patchByte(0x0043d54c + 1, 0x45);
	patchByte(0x0043d54c + 2, 0x18);
	patchNop(0x0043d558, 2);
	patchNop(0x0043d55d, 6);
	patchCall(0x0043d55d, SetTextureStageStateWrapper_SetAnisotropy);

	patchNop(0x0043d563, 5);
	patchByte(0x0043d563, 0x8a);
	patchByte(0x0043d563 + 1, 0x45);
	patchByte(0x0043d563 + 2, 0x18);
	patchNop(0x0043d56e, 2);
	patchNop(0x0043d571, 6);
	patchCall(0x0043d571, SetTextureStageStateWrapper_SetAnisotropy);

	// after drawing
	patchNop(0x0043d6e3, 2);

	patchNop(0x0043d6e9, 5);
	patchByte(0x0043d6e9, 0x8a);
	patchByte(0x0043d6e9 + 1, 0x45);
	patchByte(0x0043d6e9 + 2, 0x18);
	patchNop(0x0043d6f1, 2);
	patchNop(0x0043d6f6, 6);
	patchCall(0x0043d6f6, SetTextureStageStateWrapper_SetAnisotropy);
}

void renderWorldWrapper() {
	void (*renderWorld)() = 0x00461950;
	renderWorld();

	uint32_t *conv_x_offset = 0x00a72a04;
	uint32_t *conv_y_offset = 0x00a72a08;
	float *conv_x_multiplier = 0x00a729fc;
	float *conv_y_multiplier = 0x00a72a00;
	uint32_t *backbuffer_width = 0x00a729f0;
	uint32_t *backbuffer_height = 0x00a729f4;

	*conv_x_multiplier = (float)(*backbuffer_width) / 704.0f;
	*conv_y_multiplier = (float)(*backbuffer_height) / 448.0f;
	*conv_x_offset = 0;
	*conv_x_offset = 32 * *conv_x_multiplier;
	*conv_y_offset = 16 * *conv_y_multiplier;
	*conv_y_offset = 0;
}

void patchUIPositioning() {
	patchCall(0x0042944e, renderWorldWrapper);
}

struct movieVertex {
	float x, y, z, w;
	uint32_t color;
	float u, v;
};

void __fastcall movie_drawPrim(uint32_t* dev, void* unused, uint32_t** alsodev, D3DPRIMITIVETYPE type, uint32_t numPrims, struct movieVertex* vertices, uint32_t stride) {
	uint32_t** actual_device = 0x00aab478;
	void(__fastcall * drawPrimitiveUP)(void*, void*, void*, uint32_t, uint32_t, struct movieVertex*, uint32_t) = (*alsodev)[72];

	uint32_t* backbuffer_height = 0x00a729f4;
	uint32_t* backbuffer_width = 0x00a729f0;

	//float targetAspect = (float)movie_width / (float)movie_height;
	float targetAspect = 4.0f / 3.0f;	// while my programmer brain says "respect the original size!" the devs said "what if the credits were anamorphic 1:1"
	float backbufferAspect = (float)*backbuffer_width / (float)*backbuffer_height;

	float target_width = *backbuffer_width;
	float target_height = *backbuffer_height;
	float target_offset_x = 0.0f;
	float target_offset_y = 0.0f;

	if (backbufferAspect > targetAspect) {
		target_width = (targetAspect / backbufferAspect) * *backbuffer_width;
		target_offset_x = (*backbuffer_width - target_width) / 2;
	}
	else if (backbufferAspect < targetAspect) {
		target_height = (backbufferAspect / targetAspect) * *backbuffer_height;
		target_offset_y = (*backbuffer_height - target_height) / 2;
	}

	vertices[0].x = target_offset_x;
	vertices[0].y = target_offset_y;

	vertices[1].x = target_offset_x + target_width;
	vertices[1].y = target_offset_y;

	vertices[2].x = target_offset_x;
	vertices[2].y = target_offset_y + target_height;

	vertices[3].x = target_offset_x + target_width;
	vertices[3].y = target_offset_y + target_height;

	drawPrimitiveUP(dev, unused, alsodev, type, numPrims, vertices, stride);
}

void patchMovieBlackBars() {
	patchNop(0x0042a12c, 6);
	patchCall(0x0042a12c, movie_drawPrim);
}

void patchVertexBufferCreation() {
	// change all index buffers to D3DPOOL_MANAGED, make them non-dynamic
	patchByte(0x0043e2e7 + 1, 0x01);	// change mesh index buffer pool to D3DPOOL_MANAGED
	//patchByte(0x0043e016 + 1, 0x01);
	//patchByte(0x00444341 + 1, 0x01);
	//patchByte(0x0044444c + 1, 0x01);

	// remove D3DUSAGE_DYNAMIC flag from index buffer usage
	//patchDWord(0x0043e01c + 1, 0);	
	patchDWord(0x0043e2f9 + 1, 0);	// remove D3DUSAGE_DYNAMIC flag from index buffer usage
	//patchDWord(0x00444347 + 1, 0);
	//patchDWord(0x00444452 + 1, 0);
}

