
//**************************************************************************
//**
//** MAIN.C
//**
//** Target:		DGL Driver for OpenGL
//** Description:	Init and shutdown, state management
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "drOpenGL.h"

#include <stdlib.h>

/*
//#define DEBUGMODE
#ifdef DEBUGMODE
#	define DEBUG
#	include <assert.h>
#	include <dprintf.h>
#endif*/

// MACROS ------------------------------------------------------------------

#if DRMESA
#define RENDER_WIREFRAME
#endif

// A helpful macro that changes the origin of the screen
// coordinate system.
#define FLIP(y)	(screenHeight - (y+1))

// TYPES -------------------------------------------------------------------

// FUNCTION PROTOTYPES -----------------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean			firstTimeInit = true;

// The State.
HWND			hwnd;
//HDC				hdc;
HGLRC			hglrc;
int				screenWidth, screenHeight, screenBits, windowed;
DGLuint			currentTex;
int				palExtAvailable, sharedPalExtAvailable;
boolean			texCoordPtrEnabled;
int				maxTexSize;
float			maxAniso = 1;
int				useAnisotropic;
float			nearClip, farClip;
int				useFog;
int				verbose, noArrays = true;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//===========================================================================
// fullscreenMode
//	Change the display mode using the Win32 API.
//===========================================================================
int fullscreenMode(int width, int height, int bpp)
{
	DEVMODE	newMode;
	int		res;

	// Switch to the requested resolution.
	memset(&newMode, 0, sizeof(newMode));	// Clear the structure.
	newMode.dmSize = sizeof(newMode);
	newMode.dmPelsWidth = width;
	newMode.dmPelsHeight = height;
	newMode.dmBitsPerPel = bpp;
	newMode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;
	if(bpp) newMode.dmFields |= DM_BITSPERPEL;
	if((res=ChangeDisplaySettings(&newMode, 0)) != DISP_CHANGE_SUCCESSFUL)
	{
		Con_Message("drOpenGL.setResolution: Error %d.\n", res);
		return 0; // Failed, damn you.
	}

	// Set the correct window style and size.
	SetWindowLong(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE 
		| WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
	SetWindowPos(hwnd, 0, 0, 0, width, height, SWP_NOZORDER);

	// Update the screen size variables.
	screenWidth = width;
	screenHeight = height;
	if(bpp) screenBits = bpp;

	// Done!
	return 1;
}

//===========================================================================
// windowedMode
//	Only adjusts the window style and size.
//===========================================================================
void windowedMode(int width, int height)
{
	// We need to have a large enough client area.
	RECT rect;
	int xoff = (GetSystemMetrics(SM_CXSCREEN) - width) / 2, 
		yoff = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
	LONG style;
	
	if(ArgCheck("-nocenter")) xoff = yoff = 0;
	if(ArgCheckWith("-xpos", 1)) xoff = atoi(ArgNext());
	if(ArgCheckWith("-ypos", 1)) yoff = atoi(ArgNext());
	
	rect.left = xoff;
	rect.top = yoff;
	rect.right = xoff + width;
	rect.bottom = yoff + height;

	// Set window style.
	style = GetWindowLong(hwnd, GWL_STYLE) 
		| WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE | WS_CAPTION 
		| WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
	SetWindowLong(hwnd, GWL_STYLE, style);
	AdjustWindowRect(&rect, style, FALSE);
	SetWindowPos(hwnd, 0, xoff, yoff, /*rect.left, rect.top,*/ 
		rect.right-rect.left, rect.bottom-rect.top, SWP_NOZORDER);

	screenWidth = width;
	screenHeight = height;
}

//===========================================================================
// initState
//===========================================================================
void initState()
{
	GLfloat fogcol[4] = { .54f, .54f, .54f, 1 };

	nearClip = 5;
	farClip = 8000;	
	currentTex = 0;
	polyCounter = 0;

	usePalTex = DGL_FALSE;
	dumpTextures = DGL_FALSE;
	useCompr = DGL_FALSE;

	// Here we configure the OpenGL state and set projection matrix.
	glFrontFace(GL_CW);
	glDisable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glDisable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
#ifndef DRMESA
	glEnable(GL_TEXTURE_2D);
#else
	glDisable(GL_TEXTURE_2D);
#endif

	// The projection matrix.
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	// Initialize the modelview matrix.
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	// Also clear the texture matrix.
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();

	// Alpha blending is a go!
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0);

	// Default state for the white fog is off.
	useFog = 0;
	glDisable(GL_FOG);
	glFogi(GL_FOG_MODE, GL_LINEAR);
	glFogi(GL_FOG_END, 2100);	// This should be tweaked a bit.
	glFogfv(GL_FOG_COLOR, fogcol);

	if(!noArrays)
	{
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_COLOR_ARRAY);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		texCoordPtrEnabled = false;
	}

#if DRMESA
	glDisable(GL_DITHER);
	glDisable(GL_LIGHTING);
	glDisable(GL_LINE_SMOOTH);
	glDisable(GL_POINT_SMOOTH);
	glDisable(GL_POLYGON_SMOOTH);
	glShadeModel(GL_FLAT);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
#else
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
#endif

#ifdef RENDER_WIREFRAME
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
#else
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif
}

//===========================================================================
// initOpenGL
//===========================================================================
int initOpenGL()
{
	HDC hdc = GetDC(hwnd);

	// Create the OpenGL rendering context.
	if(!(hglrc = wglCreateContext(hdc)))
	{
		int res = GetLastError();
		Con_Message("drOpenGL.initOpenGL: Creation of rendering context failed. Error %d.\n",res);
		return 0;
	}

	// Make the context current.
	if(!wglMakeCurrent(hdc, hglrc))
	{
		Con_Message("drOpenGL.initOpenGL: Couldn't make the rendering context current.\n");
		return 0;
	}

	ReleaseDC(hwnd, hdc);

	initState();
	return 1;
}


// THE ROUTINES ------------------------------------------------------------

//===========================================================================
// DG_Init
//	'mode' is either DGL_MODE_WINDOW or DGL_MODE_FULLSCREEN. If 'bpp' is
//	zero, the current display color depth is used.
//===========================================================================
int DG_Init(int width, int height, int bpp, int mode)
{
	boolean fullscreen = (mode == DGL_MODE_FULLSCREEN);
	char	*token, *extbuf;
	int		res, pixForm;
	PIXELFORMATDESCRIPTOR pfd = 
#ifndef DRMESA
	{
		sizeof(PIXELFORMATDESCRIPTOR),	// The size
		1,								// Version
		PFD_DRAW_TO_WINDOW |			// Support flags
			PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
		PFD_TYPE_RGBA,					// Pixel type
		32,								// Bits per pixel
		0,0, 0,0, 0,0, 0,0,
		0,0,0,0,0,
		32,								// Depth bits
		0,0,
		0,								// Layer type (ignored?)
		0,0,0,0
	};
#else
   /* Double Buffer, no alpha */
    {	sizeof(PIXELFORMATDESCRIPTOR),	1,
        PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_GENERIC_FORMAT|PFD_DOUBLEBUFFER|PFD_SWAP_COPY,
        PFD_TYPE_RGBA,
        24,	8,	0,	8,	8,	8,	16,	0,	0,
        0,	0,	0,	0,	0,	16,	8,	0,	0,	0,	0,	0,	0 
	};
#endif
	HWND hDesktop = GetDesktopWindow();
	HDC desktop_hdc = GetDC(hDesktop), hdc;
	int deskbpp = GetDeviceCaps(desktop_hdc, PLANES) 
		* GetDeviceCaps(desktop_hdc, BITSPIXEL);

	ReleaseDC(hDesktop, desktop_hdc);

	Con_Message("DG_Init: OpenGL.\n");

	// Are we in range here?
	if(!fullscreen)
	{
		if(width > GetSystemMetrics(SM_CXSCREEN))
			width = GetSystemMetrics(SM_CXSCREEN);
	
		if(height >	GetSystemMetrics(SM_CYSCREEN))
			height = GetSystemMetrics(SM_CYSCREEN);
	}
	
	screenWidth = width;
	screenHeight = height;
	screenBits = deskbpp;
	windowed = !fullscreen;
	
	verbose = ArgExists("-verbose");
	noArrays = !ArgExists("-vtxar");
	
	if(fullscreen)
	{
		if(!fullscreenMode(screenWidth, screenHeight, bpp))		
		{
			Con_Error("drOpenGL.Init: Resolution change failed (%d x %d).\n",
				screenWidth, screenHeight);
		}
	}
	else
	{
		windowedMode(screenWidth, screenHeight);
	}	

	// Get the device context handle.
	hdc = GetDC(hwnd);

	// Set the pixel format for the device context. This can only be done once.
	// (Windows...).
	pixForm = ChoosePixelFormat(hdc, &pfd);
	if(!pixForm)
	{
		res = GetLastError();
		Con_Error("drOpenGL.Init: Choosing of pixel format failed. Error %d.\n",res);
	}

	// Make sure that the driver is hardware-accelerated.
	DescribePixelFormat(hdc, pixForm, sizeof(pfd), &pfd);
	if(pfd.dwFlags & PFD_GENERIC_FORMAT && !ArgCheck("-allowsoftware"))
	{
		Con_Error("drOpenGL.Init: OpenGL driver not accelerated!\nUse the -allowsoftware option to bypass this.\n");
	}

	/*if(!*/
	SetPixelFormat(hdc, pixForm, &pfd);
	/*{
		res = GetLastError();
		Con_Printf("Warning: Setting of pixel format failed. Error %d.\n",res);
	}*/
	ReleaseDC(hwnd, hdc);

	if(!initOpenGL()) Con_Error("drOpenGL.Init: OpenGL init failed.\n");

	// Clear the buffers.
	DG_Clear(DGL_COLOR_BUFFER_BIT | DGL_DEPTH_BUFFER_BIT);

	token = (char*) glGetString(GL_EXTENSIONS);
	extbuf = malloc(strlen(token) + 1);
	strcpy(extbuf, token);	

	// Check the maximum texture size.
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexSize);

	InitVertexStack();
	initExtensions();
	
	if(firstTimeInit)
	{
		firstTimeInit = DGL_FALSE;
		// Print some OpenGL information (console must be initialized by now).
		Con_Message("OpenGL information:\n");
		Con_Message("  Vendor: %s\n", glGetString(GL_VENDOR));
		Con_Message("  Renderer: %s\n", glGetString(GL_RENDERER));
		Con_Message("  Version: %s\n", glGetString(GL_VERSION));
		Con_Message("  Extensions:\n");

		// Show the list of GL extensions.
		token = strtok(extbuf, " ");
		while(token)
		{
			Con_Message("      "); // Indent.
			if(verbose)
			{
				// Show full names.
				Con_Message("%s\n", token);
			}
			else
			{
				// Two on one line, clamp to 30 characters.
				Con_Message("%-30.30s", token);
				token = strtok(NULL, " ");
				if(token) Con_Message(" %-30.30s", token);
				Con_Message("\n");
			}
			token = strtok(NULL, " ");
		}
		Con_Message("  GLU Version: %s\n", gluGetString(GLU_VERSION));	
		Con_Message("  Maximum texture size: %i\n", maxTexSize);
		if(extAniso)
		{
			glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
			Con_Message("  Maximum anisotropy: %g\n", maxAniso);
		}
		if(!noArrays) Con_Message("  Using vertex arrays.\n");
	}
	free(extbuf);

	if(ArgCheck("-dumptextures")) 
	{
		dumpTextures = DGL_TRUE;
		Con_Message("  Dumping textures (mipmap level zero).\n");
	}
	if(extAniso && ArgExists("-anifilter"))
	{
		useAnisotropic = DGL_TRUE;
		Con_Message("  Using anisotropic texture filtering.\n");
	}
	return DGL_OK;
}

//===========================================================================
// DG_Shutdown
//===========================================================================
void DG_Shutdown(void)
{
	KillVertexStack();

	// Delete the rendering context.
	wglMakeCurrent(NULL, NULL);
	wglDeleteContext(hglrc);

	// Go back to normal display settings.
	ChangeDisplaySettings(0, 0);
}


/*int	ChangeMode(int width, int height, int bpp, int fullscreen)
{
	if(!windowed && !fullscreen)
	{
		// We're currently in a fullscreen mode, but the caller
		// requests we change to windowed.
		ChangeDisplaySettings(0, 0);
		windowedMode(width, height);
		return DGL_OK;
	}
	if(width && height && (width != screenWidth || height != screenHeight))
	{
		if(fullscreen)
		{
			if(!fullscreenMode(width, height, bpp))
				return DGL_ERROR;
		}
		else
		{
			windowedMode(width, height);
		}
	}
	return DGL_OK;
}*/

//===========================================================================
// DG_Clear
//===========================================================================
void DG_Clear(int bufferbits)
{
	GLbitfield mask = 0;

	if(bufferbits & DGL_COLOR_BUFFER_BIT) mask |= GL_COLOR_BUFFER_BIT;
	if(bufferbits & DGL_DEPTH_BUFFER_BIT) mask |= GL_DEPTH_BUFFER_BIT;
	glClear(mask);
}

//===========================================================================
// DG_Show
//===========================================================================
void DG_Show(void)
{
	HDC hdc = GetDC(hwnd);

#ifdef DEBUGMODE
/*	glBegin(GL_LINES);
	glColor3f(1, 1, 1);
	glVertex2f(50, 50);
	glVertex2f(100, 60);
	primType = DGL_LINES;
	End();*/
	//assert(glGetError() == GL_NO_ERROR);
#endif

	// Swap buffers.
	SwapBuffers(hdc);
	ReleaseDC(hwnd, hdc);

#ifdef RENDER_WIREFRAME
	DG_Clear(DGL_COLOR_BUFFER_BIT);
#endif
}

//===========================================================================
// DG_Viewport
//===========================================================================
void DG_Viewport(int x, int y, int width, int height)
{
	glViewport(x, FLIP(y+height-1), width, height);
}

//===========================================================================
// DG_Scissor
//===========================================================================
void DG_Scissor(int x, int y, int width, int height)
{
	glScissor(x, FLIP(y+height-1), width, height);
}

//===========================================================================
// DG_GetIntegerv
//===========================================================================
int	DG_GetIntegerv(int name, int *v)
{
	int i;

	switch(name)
	{
	case DGL_VERSION:
		*v = DGL_VERSION_NUM;
		break;

	case DGL_MAX_TEXTURE_SIZE:
		*v = maxTexSize;
		break;

	case DGL_PALETTED_TEXTURES:
		*v = usePalTex;
		break;

	case DGL_PALETTED_GENMIPS:
		// We are unable to generate mipmaps for paletted textures.
		*v = DGL_FALSE;
		break;

	case DGL_SCISSOR_TEST:
		glGetIntegerv(GL_SCISSOR_TEST, v);
		break;

	case DGL_SCISSOR_BOX:
		glGetIntegerv(GL_SCISSOR_BOX, v);
		v[1] = FLIP(v[1]+v[3]-1);
		break;

	case DGL_FOG:
		*v = useFog;
		break;

	case DGL_R:
		*v = (int) (currentVertex.color[0] * 255);
		break;

	case DGL_G:
		*v = (int) (currentVertex.color[1] * 255);
		break;

	case DGL_B:
		*v = (int) (currentVertex.color[2] * 255);
		break;

	case DGL_A:
		*v = (int) (currentVertex.color[3] * 255);
		break;

	case DGL_RGBA:
		for(i = 0; i < 4; i++) v[i] = (int) (currentVertex.color[i] * 255);
		break;

	case DGL_POLY_COUNT:
		*v = polyCounter;
		polyCounter = 0;
		break;

	default:
		return DGL_ERROR;
	}
	return DGL_OK;
}

//===========================================================================
// DG_GetInteger
//===========================================================================
int DG_GetInteger(int name)
{
	int values[10];
	DG_GetIntegerv(name, values);
	return values[0];
}

//===========================================================================
// DG_SetInteger
//===========================================================================
int	DG_SetInteger(int name, int value)
{
	switch(name)
	{
	case DGL_WINDOW_HANDLE:
		hwnd = (HWND) value;
		break;

	default:
		return DGL_ERROR;
	}
	return DGL_OK;
}

//===========================================================================
// DG_GetString
//===========================================================================
char* DG_GetString(int name)
{
	switch(name)
	{
	case DGL_VERSION:
		return DROGL_VERSION_FULL;
	}
	return NULL;
}
	
//===========================================================================
// DG_Enable
//===========================================================================
int DG_Enable(int cap)
{
	GLfloat	midGray[4] = { .5f, .5f, .5f, 1 };

	switch(cap)
	{
	case DGL_TEXTURING:
#ifndef DRMESA
		glEnable(GL_TEXTURE_2D);
#endif
		break;

	case DGL_BLENDING:
		glEnable(GL_BLEND);
		break;

	case DGL_FOG:
		glEnable(GL_FOG);
		useFog = DGL_TRUE;
		break;

	case DGL_DEPTH_TEST:
		glEnable(GL_DEPTH_TEST);
		break;

	case DGL_ALPHA_TEST:
		glEnable(GL_ALPHA_TEST);
		break;

	case DGL_CULL_FACE:
		glEnable(GL_CULL_FACE);
		break;

	case DGL_SCISSOR_TEST:
		glEnable(GL_SCISSOR_TEST);
		break;

	case DGL_COLOR_WRITE:
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		break;

	case DGL_DEPTH_WRITE:
		glDepthMask(GL_TRUE);
		break;

	case DGL_PALETTED_TEXTURES:
		enablePalTexExt(DGL_TRUE);
		break;

	case DGL_DETAIL_TEXTURE_MODE:
		glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, midGray);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_INTERPOLATE_ARB);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, GL_SRC_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_CONSTANT_ARB);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_SRC_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_RGB_ARB, GL_PRIMARY_COLOR_ARB);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB_ARB, GL_SRC_COLOR);
		glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
		break;

	default:
		return DGL_FALSE;
	}
	return DGL_TRUE;
}

//===========================================================================
// DG_Disable
//===========================================================================
void DG_Disable(int cap)
{
	switch(cap)
	{
	case DGL_TEXTURING:
		glDisable(GL_TEXTURE_2D);
		break;

	case DGL_BLENDING:
		glDisable(GL_BLEND);
		break;

	case DGL_FOG:
		glDisable(GL_FOG);
		useFog = DGL_FALSE;
		break;

	case DGL_DEPTH_TEST:
		glDisable(GL_DEPTH_TEST);
		break;

	case DGL_ALPHA_TEST:
		glDisable(GL_ALPHA_TEST);
		break;

	case DGL_CULL_FACE:
		glDisable(GL_CULL_FACE);
		break;

	case DGL_SCISSOR_TEST:
		glDisable(GL_SCISSOR_TEST);
		break;

	case DGL_COLOR_WRITE:
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		break;

	case DGL_DEPTH_WRITE:
		glDepthMask(GL_FALSE);
		break;

	case DGL_PALETTED_TEXTURES:
		enablePalTexExt(DGL_FALSE);
		break;

	case DGL_DETAIL_TEXTURE_MODE:
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		break;
	}
}

//===========================================================================
// DG_Func
//===========================================================================
void DG_Func(int func, int param1, int param2)
{
	switch(func)
	{
	case DGL_BLENDING:
		glBlendFunc(param1==DGL_ZERO? GL_ZERO
			: param1==DGL_ONE? GL_ONE
			: param1==DGL_DST_COLOR? GL_DST_COLOR
			: param1==DGL_ONE_MINUS_DST_COLOR? GL_ONE_MINUS_DST_COLOR
			: param1==DGL_SRC_ALPHA? GL_SRC_ALPHA
			: param1==DGL_ONE_MINUS_SRC_ALPHA? GL_ONE_MINUS_SRC_ALPHA
			: param1==DGL_DST_ALPHA? GL_DST_ALPHA
			: param1==DGL_ONE_MINUS_DST_ALPHA? GL_ONE_MINUS_DST_ALPHA
			: param1==DGL_SRC_ALPHA_SATURATE? GL_SRC_ALPHA_SATURATE
			: GL_ZERO,

			param2==DGL_ZERO? GL_ZERO
			: param2==DGL_ONE? GL_ONE
			: param2==DGL_SRC_COLOR? GL_SRC_COLOR
			: param2==DGL_ONE_MINUS_SRC_COLOR? GL_ONE_MINUS_SRC_COLOR
			: param2==DGL_SRC_ALPHA? GL_SRC_ALPHA
			: param2==DGL_ONE_MINUS_SRC_ALPHA? GL_ONE_MINUS_SRC_ALPHA
			: param2==DGL_DST_ALPHA? GL_DST_ALPHA
			: param2==DGL_ONE_MINUS_DST_ALPHA? GL_ONE_MINUS_DST_ALPHA
			: GL_ZERO);
		break;

	case DGL_DEPTH_TEST:
		glDepthFunc(param1==DGL_NEVER? GL_NEVER
			: param1==DGL_LESS? GL_LESS
			: param1==DGL_EQUAL? GL_EQUAL
			: param1==DGL_LEQUAL? GL_LEQUAL
			: param1==DGL_GREATER? GL_GREATER
			: param1==DGL_NOTEQUAL? GL_NOTEQUAL
			: param1==DGL_GEQUAL? GL_GEQUAL
			: GL_ALWAYS);
		break;

	case DGL_ALPHA_TEST:
		glAlphaFunc(param1==DGL_NEVER? GL_NEVER
			: param1==DGL_LESS? GL_LESS
			: param1==DGL_EQUAL? GL_EQUAL
			: param1==DGL_LEQUAL? GL_LEQUAL
			: param1==DGL_GREATER? GL_GREATER
			: param1==DGL_NOTEQUAL? GL_NOTEQUAL
			: param1==DGL_GEQUAL? GL_GEQUAL
			: GL_ALWAYS,
			param2 / 255.0f);
		break;
	}
}

//===========================================================================
// DG_ZBias
//===========================================================================
void DG_ZBias(int level)
{
	glDepthRange(level*.0022f, 1);
}

//===========================================================================
// DG_MatrixMode
//===========================================================================
void DG_MatrixMode(int mode)
{
	glMatrixMode(mode==DGL_PROJECTION? GL_PROJECTION
		: mode==DGL_TEXTURE? GL_TEXTURE
		: GL_MODELVIEW);
}

//===========================================================================
// DG_PushMatrix
//===========================================================================
void DG_PushMatrix(void)
{
	glPushMatrix();
}

//===========================================================================
// DG_PopMatrix
//===========================================================================
void DG_PopMatrix(void)
{
	glPopMatrix();
}

//===========================================================================
// DG_LoadIdentity
//===========================================================================
void DG_LoadIdentity(void)
{
	glLoadIdentity();
}

//===========================================================================
// DG_Translatef
//===========================================================================
void DG_Translatef(float x, float y, float z)
{
	glTranslatef(x, y, z);
}

//===========================================================================
// DG_Rotatef
//===========================================================================
void DG_Rotatef(float angle, float x, float y, float z)
{
	glRotatef(angle, x, y, z);
}

//===========================================================================
// DG_Scalef
//===========================================================================
void DG_Scalef(float x, float y, float z)
{
	glScalef(x, y, z);
}

//===========================================================================
// DG_Ortho
//===========================================================================
void DG_Ortho(float left, float top, float right, float bottom, float znear, float zfar)
{
	glOrtho(left, right, bottom, top, znear, zfar);
}

//===========================================================================
// DG_Perspective
//===========================================================================
void DG_Perspective(float fovy, float aspect, float zNear, float zFar)
{
	gluPerspective(fovy, aspect, zNear, zFar);
}

//===========================================================================
// DG_Grab
//===========================================================================
int DG_Grab(int x, int y, int width, int height, int format, void *buffer)
{
	if(format != DGL_RGB) return DGL_UNSUPPORTED;
	// y+height-1 is the bottom edge of the rectangle. It's
	// flipped to change the origin.
	glReadPixels(x, FLIP(y+height-1), width, height, GL_RGB,
		GL_UNSIGNED_BYTE, buffer);
	return DGL_OK;
}

//===========================================================================
// DG_Fog
//===========================================================================
void DG_Fog(int pname, float param)
{
	int		iparam = (int) param;

	switch(pname)
	{
	case DGL_FOG_MODE:
		glFogi(GL_FOG_MODE, param==DGL_LINEAR? GL_LINEAR
			: param==DGL_EXP? GL_EXP
			: GL_EXP2);
		break;

	case DGL_FOG_DENSITY:
		glFogf(GL_FOG_DENSITY, param);
		break;

	case DGL_FOG_START:
		glFogf(GL_FOG_START, param);
		break;

	case DGL_FOG_END:
		glFogf(GL_FOG_END, param);
		break;

	case DGL_FOG_COLOR:
		if(iparam >= 0 && iparam < 256)
		{
			float col[4];
			int i;
			for(i=0; i<4; i++)
				col[i] = palette[iparam].color[i] / 255.0f;
			glFogfv(GL_FOG_COLOR, col);
		}
		break;
	}
}

//===========================================================================
// DG_Fogv
//===========================================================================
void DG_Fogv(int pname, void *data)
{
	float	param = *(float*) data;
	byte	*ubvparam = (byte*) data;
	float	col[4];
	int		i;

	switch(pname)
	{
	case DGL_FOG_COLOR:
		for(i=0; i<4; i++)
			col[i] = ubvparam[i] / 255.0f;
		glFogfv(GL_FOG_COLOR, col);
		break;

	default:
		DG_Fog(pname, param);
		break;
	}
}

//===========================================================================
// DG_Project
//	Clipping is performed. 
//===========================================================================
int DG_Project(int num, gl_fc3vertex_t *inVertices, gl_fc3vertex_t *outVertices)
{
	GLdouble	modelMatrix[16], projMatrix[16];
	GLint		viewport[4];
	GLdouble	x, y, z;
	int			i, numOut;
	gl_fc3vertex_t *in = inVertices, *out = outVertices;

	if(num == 0) return 0;

	// Get the data we'll need in the operation.
	glGetDoublev(GL_MODELVIEW_MATRIX, modelMatrix);
	glGetDoublev(GL_PROJECTION_MATRIX, projMatrix);
	glGetIntegerv(GL_VIEWPORT, viewport);
	for(i=numOut=0; i<num; i++, in++)
	{
		if(gluProject(in->pos[VX], in->pos[VY], in->pos[VZ],
			modelMatrix, projMatrix, viewport,
			&x, &y, &z) == GL_TRUE)
		{
			// A success: add to the out vertices.
			out->pos[VX] = (float) x;
			out->pos[VY] = (float) FLIP(y);
			out->pos[VZ] = (float) z;
			// Check that it's truly visible.
			if(out->pos[VX] < 0 || out->pos[VY] < 0 
				|| out->pos[VX] >= screenWidth 
				|| out->pos[VY] >= screenHeight) 
				continue;
			memcpy(out->color, in->color, sizeof(in->color));
			numOut++;
			out++;
		}
	}
	return numOut;
}

//===========================================================================
// DG_ReadPixels
//	NOTE: This function will not be needed any more when the halos are
//	rendered using the new method.
//===========================================================================
int DG_ReadPixels(int *inData, int format, void *pixels)
{
	int		type = inData[0], num, *coords, i;
	float	*fv = pixels;
	
	if(format != DGL_DEPTH_COMPONENT) return DGL_UNSUPPORTED;

	// Check the type.
	switch(type)
	{
	case DGL_SINGLE_PIXELS:
		num = inData[1];
		coords = inData + 2;
		for(i=0; i<num; i++, coords+=2)
		{
			glReadPixels(coords[0], FLIP(coords[1]), 1, 1,
				GL_DEPTH_COMPONENT, GL_FLOAT, fv+i);
		}
		break;
	
	case DGL_BLOCK:
		coords = inData + 1;
		glReadPixels(coords[0], FLIP(coords[1]+coords[3]-1), coords[2], coords[3],
			GL_DEPTH_COMPONENT, GL_FLOAT, pixels);
		break;

	default:
		return DGL_UNSUPPORTED;
	}
	return DGL_OK;
}
