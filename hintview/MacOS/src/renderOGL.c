/* This file is part of HINT
 * Copyright 2017-2021 Martin Ruckert, Hochschule Muenchen, Lothstrasse 64, 80336 Muenchen
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
 * OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * Except as contained in this notice, the name of the copyright holders shall
 * not be used in advertising or otherwise to promote the sale, use or other
 * dealings in this Software without prior written authorization from the
 * copyright holders.
 */

#include "basetypes.h"
#include "main.h"
#include "error.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include "hint.h"
#include "hfonts.h"
#include "hrender.h"
#include "rendernative.h"
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_ONLY_BMP
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define GL_SILENCE_DEPRECATION 1
#include <OpenGL/gl3.h>
#include <OpenGL/glext.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/CGLContext.h>

#define DEBUG 1

#ifdef DEBUG

static void checkGlError(const char *op)
{ GLint error;
  while( (error= glGetError())!= GL_NO_ERROR)
	  MESSAGE("OGL Error after %s: 0x%x\n", op, error);
}

static void printGLString(const char *name, GLenum s) {
  const GLubyte *v;
  v = glGetString(s);
  checkGlError(name);
  LOG("GL %s = %s\n", name, v);
}

#else
#define printGLString(N,S)	(void)0
#define checkGlError(OP)	(void)0
#endif

#define MAX_INFOLOG 512
static GLuint ProgramID, MatrixID, RuleID, GammaID, FGcolorID, IsImageID, ImageID=0;
static unsigned char *last_b=NULL;

#define xyID 0
#define uvID 1

#define STR(X) QUOTE(X)
#define QUOTE(X) #X
static const char VertexShader[]=
  "#version 330 core\n"
  /* Input */
  "layout(location = " STR(xyID) ") in vec2 vertexXY;\n"
  "layout(location = " STR(uvID) ") in vec2 vertexUV;\n"
  /* output */
   "out vec2 UV;\n"
  /* Constants for the current triangles */
  "uniform mat4 MVP;\n"

  "void main() {\n"
      "gl_Position = MVP * vec4(vertexXY,-1.0f,1.0f);\n"
      "UV = vertexUV;\n"
  "}\n";

  
static const char FragmentShader[]=
  "#version 330 core\n"

  /* Input */
  "in vec2 UV;\n"
  /* Output */
  "out vec4 color;\n"
  /* Constants for the current triangles */
  "uniform sampler2D theTexture;\n"
  "uniform vec4 FGcolor;\n"
  "uniform float Gamma;\n"
  "uniform int IsImage;\n"

  "void main()\n"
  "{ vec4 texColor = texture( theTexture, UV );\n"
    "if (IsImage==0) {\n"
      "color.a = pow(texColor.r*FGcolor.a,Gamma);\n"
      "color.r = FGcolor.r;\n"
      "color.g = FGcolor.g;\n"
      "color.b = FGcolor.b;\n"
    "}\n"
    "else color = texColor;\n"
  "}\n"
;

/* The Fragment shader should do Gamma correct (GC) blending.
   The theory says transform FGcolor and BGcolor to linear space
   FGlin = pow(FGcolor,2.2); BGlin=pow(BGcolor,2.2);
   BlendLin = FGlin*alpha + BGlin*(1-alpha);
   Blend = pow(BlendLin,1/2.2);
   
   But, the alpha values delivered by the freetype font rasterizer
   are actually brightness values! And our foreground and background
   values are pretty much either black (0.0) or white (1.0) or plain 
   Blue (0.0,0.0,1.0) or things like that. So pow(FGcolor,2.2) will do
   nothing or little to them. But we should transform the alpha to 
   linear space before blending. So we have
   AlphaLin = pow(alpha,2.2)
   BlendLin = FGcolor*alphaLin + BGcolor*(1-AlphaLin);
   Blend = pow(BlendLin, 1/2.2);
   The last line will be done automatically by the hardware if we specify
   an GL_FRAMBUFFER_SRGB. The second last line is the normal blending
   if we change the alpha value to linear.
   Using this will require that images are also converted to linear textures
   in sRGB space.
*/

static GLuint loadShader(GLenum type, char const *source)
{ GLuint shaderID;
  GLint result =GL_FALSE;

  /* Compile and check vertex shader */
  shaderID = glCreateShader(type);
  glShaderSource(shaderID, 1, &source , NULL);
  glCompileShader(shaderID);
  glGetShaderiv(shaderID, GL_COMPILE_STATUS, &result);
  if (!result)
  { char InfoLog[MAX_INFOLOG];
    glGetShaderInfoLog(shaderID, MAX_INFOLOG, NULL, InfoLog);
    LOG("Error compiling shader (%d): %s\n", type, InfoLog);
  }
  return shaderID;
}

static void createProgram(void)
{ GLuint vertexID=loadShader(GL_VERTEX_SHADER,VertexShader);
  GLuint fragmentID=loadShader(GL_FRAGMENT_SHADER,FragmentShader);
  GLint result=GL_FALSE;

  if (!vertexID  || !fragmentID) return;

  /* Create, linking, and check the program */
  ProgramID = glCreateProgram();
  if (ProgramID) {
    glAttachShader(ProgramID, vertexID);
    checkGlError("glAttachShader");
    glAttachShader(ProgramID, fragmentID);
    checkGlError("glAttachShader");
    glLinkProgram(ProgramID);
    glGetProgramiv(ProgramID, GL_LINK_STATUS, &result);
    if (!result)
    { char InfoLog[MAX_INFOLOG];
      glGetProgramInfoLog(ProgramID, MAX_INFOLOG, NULL, InfoLog);
      glDeleteProgram(ProgramID);
      ProgramID = 0;
      QUIT("Error linking shader program: %s\n", InfoLog);
    }
    glDetachShader(ProgramID, vertexID);
    glDetachShader(ProgramID, fragmentID);
  }
  glDeleteShader(vertexID);
  glDeleteShader(fragmentID);
  //LOG("createProgram Done\n");
}

/* the 4x4 Model-View-Projection Matrix */
static GLfloat MVP[4][4] = {
  {0.5, 0.0, 0.0, 0.0 }, // x scale
  {0.0, 0.5, 0.0, 0.0 }, // y scale
  {0.0, 0.0, 0.0, 0.0 }, // z scale
  {0.0, 0.0, 0.0, 1.0 }}; // translation x, y, z

/* The XY coordinates */
static GLfloat xy[6][2] = { 
  {-1.0f, 1.0f}, // top left
  {-1.0f, -1.0f}, //bot left
  {1.0f, -1.0f},  // bot right

  {1.0f, 1.0f}, //top right
  {-1.0f, 1.0f}, //top left
  {1.0f, -1.0f}  //bot right
};

/* The UV coordinates */
static const GLfloat uv[6][2] = { 
  {0.0f, 1.0f},  //left top
  {0.0f, 0.0f}, //left bot
  {1.0f, 0.0f}, //right bot

  {1.0f, 1.0f}, //right top 
  {0.0f, 1.0f}, //left top 
  {1.0f, 0.0f} //right bot 
};

static GLuint xybuffer, uvbuffer;


static void mkRuleTexture() /* the texture used for rules */
{
#define NO_TEST 1  
#if NO_TEST	
  GLubyte rule[1][1][1]={{{0xFF}}};
  /* for testing only */
#else /* for testing only */
  GLubyte rule[2][2][3]={{{0}}};
  rule[0][0][0]=0xFF; // red top left
  rule[0][1][0]=rule[0][1][1]=0xFF; // yellow top right
  rule[1][0][2]=0xFF; // blue bottom left
  rule[1][1][1]=0xFF; // green bottom right
#endif
	// Create one OpenGL texture
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  checkGlError("glPixelStorei");

  glGenTextures(1, &RuleID);
  glBindTexture(GL_TEXTURE_2D, RuleID);
  checkGlError("glBindTexture RuleID");

#if NO_TEST
  glTexImage2D(GL_TEXTURE_2D, 0,GL_R8, 1, 1 , 0, GL_RED, GL_UNSIGNED_BYTE, rule);
#else
  glTexImage2D(GL_TEXTURE_2D, 0,GL_RGB, 2, 2 , 0, GL_RGB, GL_UNSIGNED_BYTE, rule);
#endif
    checkGlError("glTeXImage2D Rule");
  /* Set texture options */
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

static ColorSet *cur_colorset=NULL;

extern void nativeInit(void)
{ GLuint VertexArrayID;
  while(glGetError()!= GL_NO_ERROR)
    continue; /* ignore errors in glewInit */
  checkGlError("starting nativeInit");
  /* Create the vertex array object (VAO) */
  glGenVertexArrays(1, &VertexArrayID);
  checkGlError("glGenVertexArrays");
  glBindVertexArray(VertexArrayID);
  checkGlError("glBindVertexArray");
  createProgram();

  MatrixID = glGetUniformLocation(ProgramID, "MVP");
  FGcolorID = glGetUniformLocation(ProgramID, "FGcolor");
  GammaID  = glGetUniformLocation(ProgramID, "Gamma");
  IsImageID = glGetUniformLocation(ProgramID, "IsImage");
  checkGlError("get IDs"); 

  glGenBuffers(1, &xybuffer);
  glBindBuffer(GL_ARRAY_BUFFER, xybuffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(xy), xy, GL_STREAM_DRAW);
  
  glGenBuffers(1, &uvbuffer);
  glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(uv), uv, GL_STATIC_DRAW);

  glUseProgram(ProgramID);


  glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVP[0][0]);
  glUniform1f(GammaID, 1.0f/2.2f);
  glUniform1i(IsImageID, 0);
  glUniform4f(FGcolorID, 0.0, 0.0, 0.0, 1.0); // black as default foreground
  glClearColor(1.0f, 1.0f, 1.0f, 1.0f); // white as default background
  cur_colorset=color_defaults;
   /* 1rst attribute buffer : XYs */
  glEnableVertexAttribArray(xyID);
  glBindBuffer(GL_ARRAY_BUFFER, xybuffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(xy), xy, GL_STREAM_DRAW);
  glVertexAttribPointer(xyID, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
  checkGlError("glVertexAttribPointer xy");

  /* 2nd attribute buffer : UVs */
  glEnableVertexAttribArray(uvID);
  glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(uv), uv, GL_STATIC_READ);
  glVertexAttribPointer(uvID,2,GL_FLOAT,GL_FALSE, 0, (void*)0);
  checkGlError("glVertexAttribPointer uv");

  /* Make the alpha channel work */
  glEnable(GL_BLEND);
  checkGlError("glEnable BLEND");
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  checkGlError("glBlendFunc");
  glEnable(GL_MULTISAMPLE);
  checkGlError("GL_MULTISAMPLE");
  glEnable(GL_FRAMEBUFFER_SRGB);
  checkGlError("GL_FRAMEBUFFER_SRGB");
    { int error=glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
        if (error!=GL_FRAMEBUFFER_COMPLETE )
            MESSAGE("framebuffer status error 0x%x",error);
    }
            
  mkRuleTexture();
  ImageID=0;
  last_b=NULL;
  //LOG("nativeInit done\n");
}


void nativeClear(void)
{ glDeleteBuffers(1, &xybuffer);
  glDeleteBuffers(1, &uvbuffer);
  glDeleteProgram(ProgramID);
  glDeleteTextures(1, &RuleID);
  if (ImageID != 0) {
        glDeleteTextures(1, &ImageID);
        ImageID = 0; last_b=NULL;
  }
}

void nativeSetGamma(double gamma)
{ glUniform1f(GammaID, 1.0/gamma);
  checkGlError("glsetgamma");
}


static uint32_t cur_fg=0;

static void nativeSetForeground(uint32_t fg)
/* set foreground rgba colors */
{ if (fg!=cur_fg)
  { uint8_t r,g,b,a;
    cur_fg=fg;
    a=fg&0xFF;fg=fg>>8;
    b=fg&0xFF;fg=fg>>8;
    g=fg&0xFF;fg=fg>>8;
    r=fg&0xFF;
    glUniform4f(FGcolorID, r/255.0f,g/255.0f,b/255.0f,a/255.0f);
  }
}

void nativeBackground(double x, double y, double w, double h)
{ uint32_t bg, fg;
  fg=cur_colorset[0][cur_mode*6+cur_style*2];
  bg=cur_colorset[0][cur_mode*6+cur_style*2+1];
  nativeSetForeground(bg);
  nativeRule(x,y,w,h);
  nativeSetForeground(fg);
}

void nativeSetDark(int on)
{ uint32_t fg;
  cur_mode=on?1:0;
  fg=cur_colorset[0][cur_mode*6+cur_style*2];
  nativeSetForeground(fg);
}

void nativeSetColor(ColorSet *cs)
{ cur_colorset=cs;
  nativeSetDark(cur_mode);
}

void nativeBlank(void)
{ uint32_t bg;
  uint8_t r,g,b,a;
  if (cur_colorset==NULL)
    QUIT("Calling nativeBlank without calling nativeSetColor");
  bg=cur_colorset[0][cur_mode*6+1];
  a=bg&0xFF;bg=bg>>8;
  b=bg&0xFF;bg=bg>>8;
  g=bg&0xFF;bg=bg>>8;
  r=bg&0xFF;
  glClearColor(	r/255.0f,g/255.0f,b/255.0f,a/255.0f);
  glClear(GL_COLOR_BUFFER_BIT);
}


static float pt_h=600.0, pt_v=800.0;

void nativeSetSize(int px_h, int px_v, double x_dpi, double y_dpi)
{  //LOG("xdpi=%f, ydpi=%f\n",x_dpi,y_dpi);
   pt_h = px_h * 72.27 / x_dpi;
   pt_v = px_v * 72.27 / y_dpi; 
   page_h = round(pt_h * (1 << 16));
   page_v = round(pt_v * (1 << 16));
   MVP[0][0]=2.0/pt_h; // x: scale to -1 to +1
   MVP[1][1]=-2.0/pt_v; // y: scale to 1 to -1
   //MVP[2][2]=0.0f; // z: don't care
   MVP[3][0]=-1.0f; // x position: left
   MVP[3][1]=1.0f; // y position: up
   //MVP[3][2]=-1.0f; // don't care
   //MVP[3][3]=1.0f; // w: identity, already there
   glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVP[0][0]);
   glViewport(0, 0, px_h, px_v);
}

extern void nativeRule(double x, double y, double w, double h)
/* Using GL to render a rule (a black rectangle)
   Coordinates in points, origin bottom left, x and w right, y and h up
   x,y position
   w,h width and height
  */
{ /* Moving using the vertex buffer */
  xy[0][0]=x;   xy[0][1]=y;   // xy top left
  xy[1][0]=x;   xy[1][1]=y-h; // xy bot left 0
  xy[2][0]=x+w; xy[2][1]=y-h; // xy bot right 0

  xy[3][0]=x+w; xy[3][1]=y;    // xy top right 1
  xy[4][0]=x;   xy[4][1]=y;    // xy top left 1
  xy[5][0]=x+w;  xy[5][1]=y-h; // xy bot right 1
  glBindTexture(GL_TEXTURE_2D, RuleID);
  checkGlError("glBindTexture RuleID");

  glBindBuffer(GL_ARRAY_BUFFER, xybuffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(xy), xy, GL_STREAM_DRAW);
  glDrawArrays(GL_TRIANGLES, 0, 2*3);
  //LOG("nativeRule %f@%f %fx%f Done\n",x,y,w,h);
}

void nativeImage(double x, double y, double w, double h, unsigned char *b, unsigned char *e)
/* render the image found between *b and *e at x,y with size w,h.
   x, y, w, h are given in point
*/
{ GLenum format, internal_format;
  int width, height, nrChannels;

  if (b!=last_b||ImageID==0)
  { unsigned char *data;
    static unsigned char grey[4]={0,0x80,0x80,0x80};
    if (ImageID != 0) {
        glDeleteTextures(1, &ImageID);
        ImageID = 0;
    }
    last_b=b;
    //LOG("nativeImage %d chanels\n",nrChannels);

    data = stbi_load_from_memory(b, (int) (e - b), &width, &height, &nrChannels, 4);
    if (data == NULL)
    { LOG("Unable to display image\n");
      data=grey; width=height=1; nrChannels=4;
    }
 /* this works with setting the number of channels to 4 above */
    internal_format=GL_SRGB_ALPHA;
    format=GL_RGBA;

    glGenTextures(1, &ImageID);
    glBindTexture(GL_TEXTURE_2D, ImageID);
    checkGlError("glBindTexture ImageID");
    glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0,
                 format, GL_UNSIGNED_BYTE, data);
    checkGlError("glTexImage2D(image)");
    if (data!=grey) { stbi_image_free(data); data=NULL; }
    glGenerateMipmap(GL_TEXTURE_2D);
    checkGlError("glGenerateMipmap(image)");
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  }
  else
  {  glBindTexture(GL_TEXTURE_2D, ImageID);
     checkGlError("glBindTexture old ImageID");
  }

  xy[0][0]=x;	  xy[0][1]=y;
  xy[1][0]=x;	  xy[1][1]=y-h;
  xy[2][0]=x+w;   xy[2][1]=y-h;

  xy[3][0]=x+w;	  xy[3][1]=y;
  xy[4][0]=x;     xy[4][1]=y;
  xy[5][0]=x+w;   xy[5][1]=y-h;

  glBindBuffer(GL_ARRAY_BUFFER, xybuffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(xy), xy, GL_STREAM_DRAW);

  glUniform1i(IsImageID, 1);
  glDrawArrays(GL_TRIANGLES, 0, 2*3);
  glUniform1i(IsImageID, 0);
}

static const int to_nearest=1;
static void GLtexture(Gcache *g) {
    unsigned texID;
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    checkGlError("glPixelStorei");
    glGenTextures(1, &texID);
    checkGlError("glGenTextures");
    glBindTexture(GL_TEXTURE_2D, texID);
    checkGlError("glBindTexture texID");
    /* the first element in g->bits corresponds to the lower left pixel,
     * the last element in g->bits to the upper right pixel. */
    glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_R8,
            g->w,
            g->h,
            0,
            GL_RED,
            GL_UNSIGNED_BYTE,
            g->bits
    );
    checkGlError("glTeXImage2D Glyph");

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if (to_nearest) 
    { glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    else
    { glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    g->GLtexture = texID;
    //MESSAGE("Generated GL texture %d",g->GLtexture);
}

void nativeSetPK(struct gcache_s*g)
{GLtexture(g); /* can I free g->bits ? */}

void nativeSetFreeType(struct gcache_s*g)
{GLtexture(g);}
extern int drag;
int round_to_pixel=1; /* makes sense only if using the native dpi, if using a multiple its of not much use*/
double pixel_size_threshold= 72.27/200; /*round to pixel only if pixel size in pt is above threshold*/
void nativeGlyph(double x, double dx, double y, double dy, double w, double h, struct gcache_s *g, int s)
/* given glyph g, display g at position x,y in size w,h. x, y, w, h are given in point */
{  
  if (g->GLtexture == 0)
    GLtexture(g);
  x=x-dx;
  y=y+h-dy;
  if (round_to_pixel && !drag)
  { double pxs;
    pxs = 72.27/xdpi; /* pixel size in point */
    if (pxs>=pixel_size_threshold)
    { x=x/pxs;
      x=floor(x+0.5);
      x=x*pxs;
    }
    pxs = 72.27/ydpi; /* pixel size in point */
    if (pxs>=pixel_size_threshold)
    { y=y/pxs;
      y=floor(y+0.5);
      y=y*pxs;
    }
  }
  xy[0][0]=x;	  xy[0][1]=y;
  xy[1][0]=x;	  xy[1][1]=y-h;
  xy[2][0]=x+w;     xy[2][1]=y-h;

  xy[3][0]=x+w;	  xy[3][1]=y;
  xy[4][0]=x;       xy[4][1]=y;
  xy[5][0]=x+w;     xy[5][1]=y-h;

  glBindTexture(GL_TEXTURE_2D, g->GLtexture);
  checkGlError("glBindTexture g->GLtexture");

  nativeSetForeground(cur_colorset[0][cur_mode*6+s*2]);

  glBindBuffer(GL_ARRAY_BUFFER, xybuffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(xy), xy, GL_STREAM_DRAW);
  glDrawArrays(GL_TRIANGLES, 0, 2*3);
  //checkGlError("glDrawArrays");
}

void nativeFreeGlyph(struct gcache_s*g)
{    if (g->GLtexture != 0) {
        glDeleteTextures(1, &(g->GLtexture)); // probably not needed
        g->GLtexture = 0;
    }
}


static int print_w, print_h, print_bpr, print_bpp, print_page;
    GLuint FramebufferID = 0;
    GLuint PrintTextureID =0;
    GLuint depthrenderbuffer;
     GLenum DrawBuffers[1] = {GL_COLOR_ATTACHMENT0};

int nativePrintStart(int w, int h, int bpr, int bpp, unsigned char *bitdata)
{ print_w=w;
  print_h=h;
    print_bpr=bpr;
    print_bpp=bpp;
  print_page=0;
      glGenFramebuffers(1, &FramebufferID);
    glBindFramebuffer(GL_FRAMEBUFFER, FramebufferID);
    
    /* texture */
    glGenTextures(1, &PrintTextureID);
    glBindTexture(GL_TEXTURE_2D, PrintTextureID);
    glTexImage2D(GL_TEXTURE_2D, 0,GL_RGB, print_w, print_h, 0,GL_RGB, GL_UNSIGNED_BYTE, 0);
    /* filtering */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#if 0
    /* depth buffer  possibly not needed*/
    glGenRenderbuffers(1, &depthrenderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, depthrenderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, print_w, print_h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthrenderbuffer);
#endif
    /* set texture as color attachment */
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, PrintTextureID, 0);
    glDrawBuffers(1, DrawBuffers); // "1" is the size of DrawBuffers

    // Always check that our framebuffer is ok
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        return 0;
    MVP[0][0]=2.0/pt_h; // x: scale to -1 to +1
    MVP[1][1]=2.0/pt_v; // y: scale to -1 to +1
    //MVP[2][2]=0.0f; // z: don't care
    MVP[3][0]=-1.0f; // x position: left
    MVP[3][1]=-1.0f; // y position: down
    //MVP[3][2]=-1.0f; // don't care
    //MVP[3][3]=1.0f; // w: identity, already there
    glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVP[0][0]);
 
    return 1;
}

int nativePrint(unsigned char *bitdata)
{    print_page++;
#if 0 /* pseudo printing */
    int x,y;
  
    memset(bitdata,0xFF&(0x20*print_page),print_bpr*print_h);
    for (x=0; x<print_h;x=x+1)
         for (y=0;y<print_bpr;y=y+print_bpp)
         {  bitdata[x*print_bpr +y]=(y*0xFF/print_bpr)&0xFF; /* red */
             bitdata[x*print_bpr +y+3]=0xFF; /* alpha */
         }
#endif

    /* renering into a framebuffer */

    GLint v[4];
    glGetIntegerv(GL_VIEWPORT,v);
    //memset(bitdata,0x80,print_h*print_bpr); // v[2]*v[3]*4
    glPixelStorei(GL_PACK_ALIGNMENT,4);
   // glPixelStorei(GL_PACK_ROW_LENGTH,print_bpr/print_bpp);

    glReadPixels(0, 0, v[2], v[3], GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid*)bitdata);
    checkGlError("glReadPixel");

    return 1;
}

int nativePrintEnd(void)
{
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glDeleteFramebuffers(1, &FramebufferID);
  glDeleteTextures(1, &PrintTextureID);
  glDeleteRenderbuffers(1, &depthrenderbuffer);
    return 0;
}
