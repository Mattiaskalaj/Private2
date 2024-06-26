/** 
 * @file spotLightF.glsl
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

uniform sampler2DRect diffuseRect;
uniform sampler2DRect specularRect;
uniform sampler2DRect depthMap;
uniform sampler2DRect normalMap;
uniform sampler2D noiseMap;
uniform sampler2D projectionMap;

uniform mat4 proj_mat; //screen space to light space
uniform float proj_near; //near clip for projection
uniform vec3 proj_p; //plane projection is emitting from (in screen space)
uniform vec3 proj_n;
uniform float proj_focus; //distance from plane to begin blurring
uniform float proj_lod;  //(number of mips in proj map)
uniform float proj_range; //range between near clip and far clip plane of projection
uniform float proj_ambiance;
uniform float near_clip;
uniform float far_clip;

uniform vec3 proj_origin; //origin of projection to be used for angular attenuation
uniform float sun_wash;

uniform vec3 center;
uniform vec3 color;
uniform float falloff;
uniform float size;

VARYING vec4 vary_fragcoord;
uniform vec2 screen_res;

uniform mat4 inv_proj;
uniform float exo_gammafactor;

vec4 getPosition(vec2 pos_screen)
{
	float depth = texture2DRect(depthMap, pos_screen.xy).a;
	vec2 sc = pos_screen.xy*2.0;
	sc /= screen_res;
	sc -= vec2(1.0,1.0);
	vec4 ndc = vec4(sc.x, sc.y, 2.0*depth-1.0, 1.0);
	vec4 pos = inv_proj * ndc;
	pos /= pos.w;
	pos.w = 1.0;
	return pos;
}

void main() 
{
	vec4 frag = vary_fragcoord;
	frag.xyz /= frag.w;
	frag.xyz = frag.xyz*0.5+0.5;
	frag.xy *= screen_res;
	
	vec3 pos = getPosition(frag.xy).xyz;
	vec3 lv = center.xyz-pos.xyz;
	float dist2 = dot(lv,lv);
	dist2 /= size;
	if (dist2 > 1.0)
	{
		discard;
	}
	
	vec3 norm = texture2DRect(normalMap, frag.xy).xyz;
	norm = vec3((norm.xy-0.5)*2.0,norm.z); // unpack norm
	
	norm = normalize(norm);
	float l_dist = -dot(lv, proj_n);
	
	vec4 proj_tc = (proj_mat * vec4(pos.xyz, 1.0));
	if (proj_tc.z < 0.0)
	{
		discard;
	}
	
	proj_tc.xyz /= proj_tc.w;
	
	float fa = falloff+1.0;
	float dist_atten = clamp(1.0-(dist2-1.0*(1.0-fa))/fa, 0.0, 1.0);
	
	lv = proj_origin-pos.xyz;
	lv = normalize(lv);
	float da = dot(norm, lv);
		
	vec3 col = vec3(0,0,0);
		
	vec3 diff_tex = texture2DRect(diffuseRect, frag.xy).rgb;
		
	float noise = texture2D(noiseMap, frag.xy/128.0).b;
	if (proj_tc.z > 0.0 &&
		proj_tc.x < 1.0 &&
		proj_tc.y < 1.0 &&
		proj_tc.x > 0.0 &&
		proj_tc.y > 0.0)
	{
		float lit = 0.0;
		if (da > 0.0)
		{
			float diff = clamp((l_dist-proj_focus)/proj_range, 0.0, 1.0);
			float lod = diff * proj_lod;
			
			vec4 plcol = texture2DLod(projectionMap, proj_tc.xy, lod);
			plcol.rgb = 0.755 * (plcol.rgb * plcol.rgb) + 0.245 * (plcol.rgb * plcol.rgb * plcol.rgb);
			vec3 lcol = color.rgb * plcol.rgb * plcol.a;
			
			lit = da * dist_atten * noise;
			
			col = lcol*lit*diff_tex;
		}
		
		float diff = clamp((proj_range-proj_focus)/proj_range, 0.0, 1.0);
		float lod = diff * proj_lod;
		vec4 amb_plcol = texture2DLod(projectionMap, proj_tc.xy, lod);
		amb_plcol.rgb = 0.755 * (amb_plcol.rgb * amb_plcol.rgb) + 0.245 * (amb_plcol.rgb * amb_plcol.rgb * amb_plcol.rgb);
		//float amb_da = mix(proj_ambiance, proj_ambiance*max(-da, 0.0), max(da, 0.0));
		float amb_da = proj_ambiance;
		
		amb_da += (da*da*0.5+0.5)*proj_ambiance;
			
		amb_da *= dist_atten * noise;
		
		amb_da = min(amb_da, 1.0-lit);
		
		col += amb_da*color.rgb*diff_tex.rgb*amb_plcol.rgb*amb_plcol.a;
	}
	
	
	vec4 spec = texture2DRect(specularRect, frag.xy);
	if (spec.a > 0.0)
	{
		vec3 ref = reflect(normalize(pos), norm);
		
		//project from point pos in direction ref to plane proj_p, proj_n
		vec3 pdelta = proj_p-pos;
		float ds = dot(ref, proj_n);
		
		if (ds < 0.0)
		{
			vec3 pfinal = pos + ref * dot(pdelta, proj_n)/ds;
			
			vec3 stc = (proj_mat * vec4(pfinal.xyz, 1.0)).xyz;

			if (stc.z > 0.0)
			{
				stc.xy /= stc.z+proj_near;
					
				if (stc.x < 1.0 &&
					stc.y < 1.0 &&
					stc.x > 0.0 &&
					stc.y > 0.0)
				{
					vec4 scol = texture2DLod(projectionMap, stc.xy, proj_lod-spec.a*proj_lod);
					col += dist_atten*scol.rgb*color.rgb*scol.a*spec.rgb;
				}
			}
		}
	}
	
	frag_color.rgb = col;	
	frag_color.a = 0.0;
}
