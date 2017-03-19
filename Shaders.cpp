#include <string>
#include "Render.h"

// This file contains the shader program code for all shaders
string FRAG_LOGSCALE = "\
/* the input floating-point buffer texture */			\
uniform sampler2D src;						\
/* and the color table texture */				\
uniform sampler2D ctable;					\
								\
uniform vec2 logminscl;						\
uniform vec2 selminscl;						\
uniform float vdispmax;						\
								\
vec4 hsv_to_rgb(vec3 hsv)					\
{								\
    int hint = (int)floor(hsv.r);				\
    float hfrac = hsv.r-hint;					\
    hint = hint % 6;						\
    float p = hsv.b * (1.0-hsv.g);				\
    float q = hsv.b * (1.0-(hsv.g*hfrac));			\
    float t = hsv.b * (1.0-(hsv.g*(1.0-hfrac)));		\
    if (hint == 0)						\
	return vec4(hsv.b, t, p, 1);				\
    else if (hint == 1)					\
	return vec4(q, hsv.b, p, 1);				\
    else if (hint == 2)					\
	return vec4(p, hsv.b, t, 1);				\
    else if (hint == 3)					\
	return vec4(p, q, hsv.b, 1);				\
    else if (hint == 4)					\
	return vec4(t, p, hsv.b, 1);				\
    else if (hint == 5)					\
	return vec4(hsv.b, p, q, 1);				\
    return vec4(0,0,0,0);					\
}								\
\
vec4 hsl_to_rgb(vec3 hsl)					\
{								\
    if (hsl.b == 0.0)						\
	return vec4(0,0,0,1);					\
    if (hsl.g == 0.0)						\
	return vec4(hsl.b,hsl.b,hsl.b,1);				\
									\
    float t2;								\
    if (hsl.b <= 0.5)						\
	t2 = hsl.b*(1+hsl.g);					\
    else							\
	t2 = hsl.b+hsl.g-(hsl.b*hsl.g);				\
    								\
    float t1 = 2*hsl.b - t2;					\
								\
    vec3 tmp3 = vec3(hsl.r+1.0/3.0, hsl.r, hsl.r-1.0/3.0);	\
								\
    vec4 rgb;							\
								\
    for (int i=0; i<3; i++)					\
    {								\
	if (tmp3[i] < 0) tmp3[i] += 1.0;			\
	if (tmp3[i] > 1) tmp3[i] -= 1.0;			\
	if (6*tmp3[i] < 1) rgb[i] = t1 + (t2-t1)*6*tmp3[i];	\
	else if (2*tmp3[i] < 1) rgb[i] = t2;			\
	else if (3*tmp3[i] < 2) rgb[i] = t1 + (t2-t1)*((2.0/3.0)-tmp3[i])*6; \
	else rgb[i] = t1;						\
    }								\
    rgb.a = 1;							\
    return rgb;							\
}								\
								\
void main()								\
{									\
    vec4 color = texture2D(src,gl_TexCoord[0].st);			\
    float dens = clamp(log(color.r/logminscl.x)/logminscl.y,0.0,1.0);	\
    float sigma = clamp(log2(1+color.g / color.r / vdispmax),0.0,1.0);	\
    gl_FragColor = hsl_to_rgb(vec3(sigma-0.5,0.6*dens,dens*dens));	\
    /*gl_FragColor = texture2D(ctable, vec2(dens*0.99,sigma));*/	\
    /* any points selected? */						\
    if (selminscl.x > 0)						\
    {									\
	float dens = clamp(log(color.b/selminscl.x)/selminscl.y,0.0,1.0); \
	if (dens == 0.0)						\
	    gl_FragColor *= 0.35;					\
	else								\
	    gl_FragColor *= 0.5 + 0.5*dens;				\
    }									\
}									\
									\
";

// These are our min/max finding shaders
string FRAG_MINMAX = "\
uniform sampler2D tex;\
uniform int xscale;\
uniform int yscale;\
uniform int width;\
uniform int height;\
\
void main()\
{\
    int x;\
    int y;\
    vec4 color;\
    vec2 loc = vec2(gl_FragCoord.x,gl_FragCoord.y)-0.5;\
    loc.s *= xscale;\
    loc.t *= yscale;\
    loc += 0.5;\
    gl_FragColor = texture2D(tex,vec2(loc.s/width,loc.t/height));\
\
    for (x=0;x<xscale;x++)\
    {\
	for (y=0;y<yscale;y++)\
	{\
	    color = texture2D(tex,vec2((loc.s+x)/width,(loc.t+y)/height)); \
	    if (color.r != 0)						\
	    {								\
		gl_FragColor.r = min(color.r, gl_FragColor.r);		\
		gl_FragColor.g = min(color.g, gl_FragColor.g);		\
		gl_FragColor.b = max(color.b, gl_FragColor.b);		\
		gl_FragColor.a = max(color.a, gl_FragColor.a);		\
	    }								\
	}\
    }\
}\
";

// This is for the first minmax step, from original image
string FRAG_MINMAXSTART = "\
uniform sampler2D tex;						 \
uniform float width;						 \
uniform float height;						 \
								 \
void main()							 \
{								 \
    float x;							 \
    float y;							 \
    vec4 color;							 \
    vec2 loc = vec2(gl_FragCoord.x,gl_FragCoord.y)-0.5;		 \
    loc.s *= 4;							 \
    loc.t *= 4;							 \
    loc += 0.5;							 \
    gl_FragColor = vec4(0,0,0,0);				 \
    								 \
    float dx, dy;							\
    for (dx=0.0;dx<3.5;dx+=1.0)						\
    {									\
	for (dy=0.0;dy<3.5;dy+=1.0)					\
	{								\
	    color = texture2D(tex,vec2((loc.s+dx)/width,(loc.t+dy)/height)); \
	    /* if pixel is black, discard entire block */		\
	    if (color.r == 0)						\
	    {								\
		gl_FragColor.r = 1e36;					\
	    }								\
	    /* otherwise, take max of 4x4 block */			\
	    gl_FragColor.r = max(color.r, gl_FragColor.r);		\
	    gl_FragColor.g = max(color.b, gl_FragColor.g);		\
	    gl_FragColor.b = max(color.r, gl_FragColor.b);		\
	}								\
    }									\
    gl_FragColor.a = gl_FragColor.g;					\
    /* did this block have any selected in it? */			\
    if (gl_FragColor.g == 0)						\
    {									\
	gl_FragColor.g = 1e36;						\
	gl_FragColor.a = 0;						\
    }									\
}									\
";

// This is the basic point sprite shader
string VERT_PSPRITE = "\
						\
/* these three are obvious */			\
attribute vec3 pos;				\
attribute vec3 vel;				\
attribute vec3 acc;				\
/* hsml.x is current hsml, hsml.y is next */	\
attribute vec2 hsml;				\
/* clr is (densq, vdisp), nclr is next */	\
attribute vec2 clr;				\
attribute vec2 nclr;				\
/* componentwise bytes of pid */		\
attribute vec4 pid;				\
						\
uniform vec3 pmin;				\
uniform vec3 vmin;				\
uniform vec3 amin;				\
uniform float hmin;				\
uniform vec2 cmin;				\
						\
uniform vec3 pscl;				\
uniform vec3 vscl;				\
uniform vec3 ascl;				\
uniform float hscl;				\
uniform vec2 cscl;				\
						\
uniform float dt;				\
uniform float hsmlscale;			\
uniform sampler2D seltex;			\
uniform int numSel;				\
						\
int isSelected(int ipid)			\
{						\
    int beg=0;					\
    int end=numSel;				\
    int mid=numSel/2;				\
    int selpid;					\
    int n = numSel;				\
						\
    vec4 samp = texture2D(seltex, vec2(float(mid%"SELTEXDIM_STR")+0.5,		\
				       floor(float(mid/"SELTEXDIM_STR"))+0.5)/"SELTEXDIM_STR".0); \
    selpid = int(samp.r*255.0 + 0.2);					\
    selpid += 256*int(samp.g*255.0 + 0.2);				\
    selpid += 256*256*int(samp.b*255 + 0.2);				\
    selpid += 256*256*256*int(samp.a*255.0 + 0.2);			\
    									\
    while((beg <= end) && (ipid != selpid))				\
    {									\
	if (ipid < selpid)						\
	{								\
	    end = mid - 1;						\
	    n = n/2;							\
	    mid = beg + n/2;						\
	}								\
	else								\
	{								\
	    beg = mid + 1;						\
	    n = n/2;							\
	    mid = beg + n/2;						\
	}								\
	samp = texture2D(seltex, vec2(float(mid%"SELTEXDIM_STR")+0.5,		\
				      floor(float(mid/"SELTEXDIM_STR"))+0.5)/"SELTEXDIM_STR".0); \
	selpid = int(samp.r*255.0+0.2);					\
	selpid += 256*int(samp.g*255.0+0.2);				\
	selpid += 256*256*int(samp.b*255.0+0.2);			\
	selpid += 256*256*256*int(samp.a*255.0+0.2);			\
    }									\
    									\
    if (ipid == selpid)							\
	return 1;							\
									\
    return 0;								\
}									\
									\
void main()								\
{								\
    /* compute actual pid */					\
    int ipid = int(pid.x+0.2);					\
    ipid += 256*int(pid.y+0.2);				\
    ipid += 256*256*int(pid.z+0.2);				\
    ipid += 256*256*256*int(pid.w+0.2);				\
    /* scale all attr. values correctly */			\
    pos = (pos*pscl) + pmin;					\
    vel = (vel*vscl) + vmin;					\
    acc = (acc*ascl) + amin;					\
    clr = (clr*cscl) + cmin;					\
    nclr = (nclr*cscl) + cmin;					\
    hsml = (hsml*hscl) + hmin;					\
    /* interpolate density, veldisp, hsml correctly */		\
    clr = clr*(1-dt) + nclr*dt;					\
    hsml.x = hsml.x*(1-dt) + hsml.y*dt;				\
    /* these two are log-compressed */				\
    gl_FrontColor.r = exp(clr.r);				\
    gl_FrontColor.g = exp(clr.g);				\
    /* update pos by vel, acc */				\
    pos += vel*dt + 0.5*acc*dt*dt;				\
    /* transform projection to eye coords */			\
    gl_Position = gl_ModelViewMatrix*vec4(pos,1);		\
    gl_Position.w = 0;						\
    float pdist = length(gl_Position);				\
    gl_Position.w = 1;						\
    /* set point size as hsml scaled by dist (*2 for radius) */	\
    gl_PointSize = 2*hsmlscale*hsml.x/pdist;			\
    /* finish transforming pos */				\
    gl_Position = gl_ProjectionMatrix*gl_Position;		\
    /* limit point size */					\
    gl_PointSize = floor(clamp(gl_PointSize, 1.1, "PSSTR".1));	\
    /* is the point selected? */				\
    if (numSel > 0 && isSelected(ipid) != 0)			\
	gl_FrontColor.b = gl_FrontColor.r;			\
    else							\
	gl_FrontColor.b = 0;					\
    gl_FrontColor /= (pdist*pdist*pdist*pdist);				\
    /* secretly pass point size to fragment shader */		\
    gl_FrontColor.a = gl_PointSize;				\
}								\
";

// and this is the point sprite fragment shader
string FRAG_PSPRITE = "\
/* normalization factors for point sizes, up to max pt. size */	\
uniform float nfac["PSSTR"];						\
									\
void main()								\
{									\
    gl_FragColor = gl_Color;						\
    /* integer point size, to check array */				\
    /* since point size is hidden in color.a */				\
    int ind = (int)(gl_Color.a+0.5);					\
    /* set p as the pixel coordinate, going from 0...size-1 */		\
    vec2 p = floor(gl_Color.a*vec2(gl_TexCoord[0].s,gl_TexCoord[0].t)); \
    /* scale to 0...1*/							\
    p = (p+0.5) / gl_Color.a;						\
    /* and to -1...1 */							\
    p = 2*(p-0.5);							\
    /* now compute weight */						\
    float u = length(p);						\
    /* don't draw if outside a sensible radius */			\
    if (u > 1) discard;							\
									\
    float fac;								\
    if (u < 0.5)							\
	fac = (2.546479089470 + 15.278874536822*(u-1)*u*u);		\
    else								\
	fac = 5.092958178941*(1.0-u)*(1.0-u)*(1.0-u);			\
    /* compute final point color */					\
    gl_FragColor *= fac/nfac[ind-1];					\
}									\
";
