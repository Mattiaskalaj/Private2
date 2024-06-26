/** 
 * @file alphaF.glsl
 *
 * $LicenseInfo:firstyear=2007&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2007, Linden Research, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */
 
#extension GL_ARB_texture_rectangle : enable

/*[EXTRA_CODE_HERE]*/

#ifdef DEFINE_GL_FRAGCOLOR
out vec4 frag_color;
#else
#define frag_color gl_FragColor
#endif

uniform sampler2DRectShadow shadowMap0;
uniform sampler2DRectShadow shadowMap1;
uniform sampler2DRectShadow shadowMap2;
uniform sampler2DRectShadow shadowMap3;
uniform sampler2DRect depthMap;
uniform sampler2D diffuseMap;

uniform mat4 shadow_matrix[6];
uniform vec4 shadow_clip;
uniform vec2 screen_res;
uniform vec4 shadow_res;

vec3 atmosLighting(vec3 light);
vec3 scaleSoftClip(vec3 light);

VARYING vec3 vary_ambient;
VARYING vec3 vary_directional;
VARYING vec3 vary_fragcoord;
VARYING vec3 vary_position;
VARYING vec3 vary_pointlight_col;
VARYING vec2 vary_texcoord0;
VARYING vec4 vertex_color;

uniform vec4 shadow_bias;

uniform mat4 inv_proj;

vec4 getPosition(vec2 pos_screen)
{
	float depth = texture2DRect(depthMap, pos_screen.xy).a;
	vec2 sc = pos_screen.xy*2.0;
	sc /= screen_res;
	sc -= vec2(1.0,1.0);
	vec4 ndc = vec4(sc.x, sc.y, 2.0*depth-1.0, 1.0);
	vec4 pos = inv_proj * ndc;
	pos.xyz /= pos.w;
	pos.w = 1.0;
	return pos;
}

float pcfShadow(sampler2DShadow shadowMap, vec4 stc, vec2 pos_screen, float shad_res, float bias)
{
	float recip_shadow_res = 1.0 / shad_res;
	stc.xyz /= stc.w;
	stc.z += bias;
	
	stc.x = floor(stc.x*shad_res + fract(pos_screen.y*0.5)) * recip_shadow_res;
	float cs = shadow2D(shadowMap, stc.xyz).x;
	
	float shadow = cs;
	
	shadow += shadow2D(shadowMap, stc.xyz+vec3(0.60*recip_shadow_res, 0.55*recip_shadow_res, 0.0)).x;
	shadow += shadow2D(shadowMap, stc.xyz+vec3(0.72*recip_shadow_res, -0.65*recip_shadow_res, 0.0)).x;
	shadow += shadow2D(shadowMap, stc.xyz+vec3(-0.60*recip_shadow_res, 0.55*recip_shadow_res, 0.0)).x;
	shadow += shadow2D(shadowMap, stc.xyz+vec3(-0.72*recip_shadow_res, -0.65*recip_shadow_res, 0.0)).x;
	         
    return shadow*0.2;
}


void main() 
{
	vec2 frag = vary_fragcoord.xy/vary_fragcoord.z*0.5+0.5;
	frag *= screen_res;
	
	float shadow = 0.0;
	vec4 pos = vec4(vary_position, 1.0);
	
	vec4 spos = pos;
		
	if (spos.z > -shadow_clip.w)
	{	
		vec4 lpos;
		
		vec4 near_split = shadow_clip*-0.75;
		vec4 far_split = shadow_clip*-1.25;
		vec4 transition_domain = near_split-far_split;
		float weight = 0.0;

		if (spos.z < near_split.z)
		{
			lpos = shadow_matrix[3]*spos;
			
			float w = 1.0;
			w -= max(spos.z-far_split.z, 0.0)/transition_domain.z;
			shadow += pcfShadow(shadowMap3, lpos, pos_screen, shadow_res.w, shadow_bias.w)*w;
			weight += w;
			shadow += max((pos.z+shadow_clip.z)/(shadow_clip.z-shadow_clip.w)*2.0-1.0, 0.0);
		}

		if (spos.z < near_split.y && spos.z > far_split.z)
		{
			lpos = shadow_matrix[2]*spos;
			
			float w = 1.0;
			w -= max(spos.z-far_split.y, 0.0)/transition_domain.y;
			w -= max(near_split.z-spos.z, 0.0)/transition_domain.z;
			shadow += pcfShadow(shadowMap2, lpos, pos_screen, shadow_res.z, shadow_bias.z)*w;
			weight += w;
		}

		if (spos.z < near_split.x && spos.z > far_split.y)
		{
			lpos = shadow_matrix[1]*spos;
			
			float w = 1.0;
			w -= max(spos.z-far_split.x, 0.0)/transition_domain.x;
			w -= max(near_split.y-spos.z, 0.0)/transition_domain.y;
			shadow += pcfShadow(shadowMap1, lpos, pos_screen, shadow_res.y, shadow_bias.y)*w;
			weight += w;
		}

		if (spos.z > far_split.x)
		{
			lpos = shadow_matrix[0]*spos;
							
			float w = 1.0;
			w -= max(near_split.x-spos.z, 0.0)/transition_domain.x;
				
			shadow += pcfShadow(shadowMap0, lpos, pos_screen, shadow_res.x, shadow_bias.x)*w;
			weight += w;
		}
		

		shadow /= weight;

	}
	else
	{
		shadow = 1.0;
	}

	vec4 diff = texture2D(diffuseMap,vary_texcoord0.xy);
	diff.rgb = diff.rgb * diff.rgb * 1.08933 - diff.rgb * 0.0893293;

	vec4 col = vec4(vary_ambient + vary_directional.rgb*shadow, vertex_color.a);
	vec4 color = diff * col;
	
	color.rgb = atmosLighting(color.rgb);

	color.rgb = scaleSoftClip(color.rgb);

	color.rgb += diff.rgb * vary_pointlight_col.rgb;

	frag_color = color;	
}

