/* tmaptext.c */

/* Texture-mapped text */

/* fsv - 3D File System Visualizer
 * Copyright (C)1999 Daniel Richard G. <skunk@mit.edu>
 * SPDX-FileCopyrightText: 2021 Janne Blomqvist <blomqvist.janne@gmail.com>
 *
 * SPDX-License-Identifier:  LGPL-2.1-or-later
 */


#include "common.h"
#include "tmaptext.h"

#include "ogl.h"

#include <gio/gio.h>

/* Bitmap font definition */
#define char_width 16
#define char_height 32
#include "xmaps/charset.xbm"


/* Text can be squeezed to at most half its normal width */
#define TEXT_MAX_SQUEEZE 2.0


/* Normal character aspect ratio */
static const double char_aspect_ratio = (double)char_width / (double)char_height;

/* Font texture object */
static GLuint text_tobj;

// Global state for modern GL
static struct FsvGlTextState {
	// Don't need a VAO as we just use the one global VAO which is
	// always active.

	GLuint program; // Handle for the shaders

	// These _location variables are handles to input 'slots' in the
	// vertex shader.
	GLint mvp_location;
	GLint position_location;
	GLint texcoord_location;
	GLint texture_location;
	GLint color_location;

	// Projection and modelview matrices (using cglm library) from the
	// global state.
} glt;

typedef struct {
	GLfloat position[3];
	GLfloat texCoord[2];
} TextVertex;

/* Simple XBM parser - bits to bytes. Caller assumes responsibility for
 * freeing the returned pixel buffer */
static byte *
xbm_pixels( const byte *xbm_bits, int pixel_count )
{
	int in_byte = 0;
	int bitmask = 1;
	int i;
	byte *pixels;

	pixels = NEW_ARRAY(byte, pixel_count);

	for (i = 0; i < pixel_count; i++) {
		/* Note: a 1 bit is black */
		if ((int)xbm_bits[in_byte] & bitmask)
			pixels[i] = 0;
		else
			pixels[i] = 255;

		if (bitmask & 128) {
			++in_byte;
			bitmask = 1;
		}
		else
			bitmask <<= 1;
	}

	return pixels;
}


// Initialize OpenGL text shaders
static GLuint
text_init_shaders()
{
	GBytes *source;
	GLuint program = 0, vertex = 0, fragment = 0;

	/* load the vertex shader */
	source = g_resources_lookup_data("/jabl/fsv/fsv-text-vertex.glsl", 0, NULL);
	vertex = ogl_create_shader(GL_VERTEX_SHADER, g_bytes_get_data(source, NULL));
	g_bytes_unref(source);
	if (vertex == 0)
		goto out;

	/* load the fragment shader */
	source = g_resources_lookup_data("/jabl/fsv/fsv-text-fragment.glsl", 0, NULL);
	fragment = ogl_create_shader(GL_FRAGMENT_SHADER, g_bytes_get_data(source, NULL));
	g_bytes_unref(source);
	if (fragment == 0)
		goto out;

	/* link the vertex and fragment shaders together */
	program = glCreateProgram();
	glAttachShader(program, vertex);
	glAttachShader(program, fragment);
	glLinkProgram(program);

	GLint status = 0;
	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (status == GL_FALSE)
	{
		GLint log_len = 0;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_len);

		char *buffer = g_malloc(log_len + 1);
		glGetProgramInfoLog(program, log_len, NULL, buffer);

		g_error("Linking failure in program: %s", buffer);

		g_free(buffer);

		glDeleteProgram(program);
		program = 0;

		goto out;
	}

	/* get the location of the "mvp" uniform */
	glt.mvp_location = glGetUniformLocation(program, "mvp");
	glt.color_location = glGetUniformLocation(program, "color");
	glt.texture_location = glGetUniformLocation(program, "tex");

	/* get the location of the "position" and "color" attributes */
	glt.position_location = glGetAttribLocation(program, "position");
	glt.texcoord_location = glGetAttribLocation(program, "texcoord");

	/* the individual shaders can be detached and destroyed */
	glDetachShader(program, vertex);
	glDetachShader(program, fragment);

out:
	if (vertex != 0)
		glDeleteShader(vertex);
	if (fragment != 0)
		glDeleteShader(fragment);

	return program;
}

/* Initializes texture-mapping state for drawing text */
void
text_init( void )
{
	float border_color[] = { 0.0, 0.0, 0.0, 1.0 };
	byte *charset_pixels;

	/* Set up text texture object */
	glGenTextures( 1, &text_tobj );
	glBindTexture( GL_TEXTURE_2D, text_tobj );

	/* Set up texture-mapping parameters */
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );

	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	glTexParameterfv( GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border_color );

	/* Load texture */
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	charset_pixels = xbm_pixels( charset_bits, charset_width * charset_height );
	// In modern GL GL_RED is the only supported single channel texture format.
	// But actually the texture is an alpha map that decides where the color
	// (specified via a uniform) will be shown and where it will be transparent.
	// In the fragment shader the red component in the texture is swizzled to
	// the alpha component of the output color.
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, charset_width, charset_height,
		     0, GL_RED, GL_UNSIGNED_BYTE, charset_pixels);
	glGenerateMipmap(GL_TEXTURE_2D);
	xfree( charset_pixels );

	glt.program = text_init_shaders();
	if (!glt.program)
		g_error("Compiling shaders failed");
}


/* Call before drawing text */
void
text_pre( void )
{
	glDisable( GL_POLYGON_OFFSET_FILL );
	glEnable( GL_BLEND );
	glBindTexture( GL_TEXTURE_2D, text_tobj );
}


/* Call after drawing text */
void
text_post( void )
{
	glDisable( GL_BLEND );
	glEnable( GL_POLYGON_OFFSET_FILL );
	glBindTexture(GL_TEXTURE_2D, 0);
}


/* Given the length of a string, and the dimensions into which that string
 * has to be rendered, this returns the dimensions that should be used
 * for each character */
static void
get_char_dims( int len, const XYvec *max_dims, XYvec *cdims )
{
	double min_width, max_width;

	/* Maximum and minimum widths of the string if it were to occupy
	 * the full depth (y-dimension) available to it */
	max_width = (double)len * max_dims->y * char_aspect_ratio;
	min_width = max_width / TEXT_MAX_SQUEEZE;

	if (max_width > max_dims->x) {
		if (min_width > max_dims->x) {
			/* Text will span full avaiable width, squeezed
			 * horizontally as much as it can take */
			cdims->x = max_dims->x / (double)len;
			cdims->y = TEXT_MAX_SQUEEZE * cdims->x / char_aspect_ratio;
		}
		else {
			/* Text will occupy full available width and
			 * height, squeezed horizontally a bit */
			cdims->x = max_dims->x / (double)len;
			cdims->y = max_dims->y;
		}
	}
	else {
		/* Text will use full available height (characters
		 * will have their natural aspect ratio) */
		cdims->y = max_dims->y;
		cdims->x = cdims->y * char_aspect_ratio;
	}
}


/* Returns the texture-space coordinates of the bottom-left and upper-right
 * corners of the specified character (glyph) */
static void
get_char_tex_coords( int c, XYvec *t_c0, XYvec *t_c1 )
{
	static const XYvec t_char_dims = {
		(double)char_width / (double)charset_width,
		(double)char_height / (double)charset_height
	};
	XYvec gpos;
	int g;

	/* Get position of lower-left corner of glyph
	 * (in bitmap coordinates, w/origin at top-left)
	 * Note: The following code is character-set-specific */
	g = c;
	if ((g < 32) || (g > 127))
		g = 63; /* question mark */
	gpos.x = (double)(((g - 32) & 31) * char_width);
	gpos.y = (double)(((g - 32) >> 5) * char_height);

	/* Texture coordinates */
	t_c0->x = gpos.x / (double)charset_width;
	t_c1->y = gpos.y / (double)charset_height;
	t_c1->x = t_c0->x + t_char_dims.x;
	t_c0->y = t_c1->y + t_char_dims.y;
}


// Draw a set of text vertices with a specified color.
// Use indexed drawing.
// The vertices for each char must be in order
// LL - LR - UL - UR
static void
draw_text_vertices(TextVertex *tv, size_t nchars)
{
	GLsizeiptr ntv = nchars * 4;
	GLsizei idx_len = nchars * 6;
	static GLuint vbo;
	if (!vbo)
		glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(TextVertex) * ntv, tv, GL_STREAM_DRAW);

	glEnableVertexAttribArray(glt.position_location);
	glVertexAttribPointer(glt.position_location, 3, GL_FLOAT, GL_FALSE,
			      sizeof(TextVertex), (void *)offsetof(TextVertex, position));

	glEnableVertexAttribArray(glt.texcoord_location);
	glVertexAttribPointer(glt.texcoord_location, 2, GL_FLOAT, GL_FALSE,
			      sizeof(TextVertex), (void *)offsetof(TextVertex, texCoord));

	GLushort *idx = NEW_ARRAY(GLushort, idx_len);
	for (size_t i = 0; i < nchars; i++) {
		size_t j = 6 * i;  // 6 indices per char
		GLushort v = 4 * i;  // 4 Vertices per char
		// First triangle in a character
		idx[j] = v;
		idx[j + 1] = v + 1;
		idx[j + 2] = v + 2;
		// Second triangle (counter-clockwise order)
		idx[j + 3] = v + 2;
		idx[j + 4] = v + 1;
		idx[j + 5] = v + 3;
	}

	static GLuint ebo;
	if (!ebo)
		glGenBuffers(1, &ebo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLushort) * idx_len, idx, GL_STREAM_DRAW);

	glUseProgram(glt.program);

	// Set the texture unit. TODO. Move to text_init()?
	glUniform1i(glt.texture_location, 0);
	glDrawElements(GL_TRIANGLES, idx_len, GL_UNSIGNED_SHORT, 0);
	glUseProgram(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	xfree(idx);
}

/* Draws a straight line of text centered at the given position,
 * fitting within the dimensions specified */
void
text_draw_straight( const char *text, const XYZvec *text_pos, const XYvec *text_max_dims )
{
	XYvec cdims;
	XYvec t_c0, t_c1, c0, c1;
	size_t len;

	len = strlen( text );
	get_char_dims( len, text_max_dims, &cdims );

	/* Corners of first character */
	c0.x = text_pos->x - 0.5 * (double)len * cdims.x;
	c0.y = text_pos->y - 0.5 * cdims.y;
	c1.x = c0.x + cdims.x;
	c1.y = c0.y + cdims.y;

	GLsizeiptr nverts = len * 4;
	TextVertex *tv = NEW_ARRAY(TextVertex, nverts);
	for (size_t i = 0; i < len; i++) {
		get_char_tex_coords( text[i], &t_c0, &t_c1 );
		size_t j = i * 4;
		// Each char defined by corners in zigzag order
		// Lower left {pos, texcoords}
		tv[j] = (TextVertex){{c0.x, c0.y, text_pos->z}, {t_c0.x, t_c0.y}};
		// Lower right
		tv[j + 1] = (TextVertex){{c1.x, c0.y, text_pos->z}, {t_c1.x, t_c0.y}};
		// Upper left
		tv[j + 2] = (TextVertex){{c0.x, c1.y, text_pos->z}, {t_c0.x, t_c1.y}};
		// Upper right
		tv[j + 3] = (TextVertex){{c1.x, c1.y, text_pos->z}, {t_c1.x, t_c1.y}};

		c0.x = c1.x;
		c1.x += cdims.x;
	}
	draw_text_vertices(tv, len);
	xfree(tv);
}


/* Draws a straight line of text centered at the given position, rotated
 * to be tangent to a circle around the origin, and fitting within the
 * dimensions specified (which are also rotated) */
void
text_draw_straight_rotated( const char *text, const RTZvec *text_pos, const XYvec *text_max_dims )
{
	XYvec cdims;
	XYvec t_c0, t_c1, c0, c1;
	XYvec hdelta, vdelta;
	double sin_theta, cos_theta;
	size_t len;

	len = strlen( text );
	get_char_dims( len, text_max_dims, &cdims );

	sin_theta = sin( RAD(text_pos->theta) );
	cos_theta = cos( RAD(text_pos->theta) );

	/* Vector to move from one character to the next */
	hdelta.x = sin_theta * cdims.x;
	hdelta.y = - cos_theta * cdims.x;
	/* Vector to move from bottom of character to top */
	vdelta.x = cos_theta * cdims.y;
	vdelta.y = sin_theta * cdims.y;

	/* Corners of first character */
	c0.x = cos_theta * text_pos->r - 0.5 * ((double)len * hdelta.x + vdelta.x);
	c0.y = sin_theta * text_pos->r - 0.5 * ((double)len * hdelta.y + vdelta.y);
	c1.x = c0.x + hdelta.x + vdelta.x;
	c1.y = c0.y + hdelta.y + vdelta.y;

	GLsizeiptr nverts = len * 4;
	TextVertex *tv = NEW_ARRAY(TextVertex, nverts);
	for (size_t i = 0; i < len; i++) {
		get_char_tex_coords( text[i], &t_c0, &t_c1 );
		size_t j = i * 4;
		// Lower left
		tv[j] = (TextVertex){{c0.x, c0.y, text_pos->z}, {t_c0.x, t_c0.y}};
		// Lower right
		tv[j + 1] = (TextVertex){{c0.x + hdelta.x, c0.y + hdelta.y, text_pos->z},
					 {t_c1.x, t_c0.y}};
		// Upper left
		tv[j + 2] = (TextVertex){{c1.x - hdelta.x, c1.y - hdelta.y, text_pos->z},
					 {t_c0.x, t_c1.y}};
		// Upper right
		tv[j + 3] = (TextVertex){{c1.x, c1.y, text_pos->z}, {t_c1.x, t_c1.y}};

		c0.x += hdelta.x;
		c0.y += hdelta.y;
		c1.x += hdelta.x;
		c1.y += hdelta.y;
	}
	draw_text_vertices(tv, len);
	xfree(tv);
}


/* Draws a curved arc of text, occupying no more than the depth and arc
 * width specified. text_pos indicates outer edge (not center) of text */
void
text_draw_curved( const char *text, const RTZvec *text_pos, const RTvec *text_max_dims )
{
	XYvec straight_dims, cdims;
	XYvec char_pos, fwsl, bwsl;
	XYvec t_c0, t_c1;
	double char_arc_width, theta;
	double sin_theta, cos_theta;
	double text_r;
	size_t len;

	/* Convert curved dimensions to straight equivalent */
	straight_dims.x = (PI / 180.0) * text_pos->r * text_max_dims->theta;
	straight_dims.y = text_max_dims->r;

	len = strlen( text );
	get_char_dims( len, &straight_dims, &cdims );

	/* Radius of center of text line */
	text_r = text_pos->r - 0.5 * cdims.y;

	/* Arc width occupied by each character */
	char_arc_width = (180.0 / PI) * cdims.x / text_r;

	theta = text_pos->theta + 0.5 * (double)(len - 1) * char_arc_width;

	GLsizeiptr nverts = len * 4;
	TextVertex *tv = NEW_ARRAY(TextVertex, nverts);
	for (size_t i = 0; i < len; i++) {
		sin_theta = sin( RAD(theta) );
		cos_theta = cos( RAD(theta) );

		/* Center of character and deltas from center to corners */
		char_pos.x = cos_theta * text_r;
		char_pos.y = sin_theta * text_r;
		/* "forward slash / backward slash" */
		fwsl.x = 0.5 * (cdims.y * cos_theta + cdims.x * sin_theta);
		fwsl.y = 0.5 * (cdims.y * sin_theta - cdims.x * cos_theta);
		bwsl.x = 0.5 * (- cdims.y * cos_theta + cdims.x * sin_theta);
		bwsl.y = 0.5 * (- cdims.y * sin_theta - cdims.x * cos_theta);

		get_char_tex_coords( text[i], &t_c0, &t_c1 );
		size_t j = i * 4;
		// Lower left
		tv[j] = (TextVertex){{char_pos.x - fwsl.x, char_pos.y - fwsl.y, text_pos->z},
				     {t_c0.x, t_c0.y}};
		// Lower right
		tv[j + 1] = (TextVertex){{char_pos.x + bwsl.x, char_pos.y + bwsl.y, text_pos->z},
					 {t_c1.x, t_c0.y}};
		// Upper left
		tv[j + 2] = (TextVertex){{char_pos.x - bwsl.x, char_pos.y - bwsl.y, text_pos->z},
					 {t_c0.x, t_c1.y}};
		// Upper right
		tv[j + 3] = (TextVertex){{char_pos.x + fwsl.x, char_pos.y + fwsl.y, text_pos->z},
					 {t_c1.x, t_c1.y}};

		theta -= char_arc_width;
	}
	draw_text_vertices(tv, len);
	xfree(tv);
}

// Set the text color
void
text_set_color(float red, float green, float blue)
{
	glUseProgram(glt.program);
	glUniform3f(glt.color_location, red, green, blue);
	glUseProgram(0);
}


// Upload MVP matrix
void
text_upload_mvp(float* mvp)
{
	glUseProgram(glt.program);
	glUniformMatrix4fv(glt.mvp_location, 1, GL_FALSE, mvp);
	glUseProgram(0);
}

/* end tmaptext.c */
