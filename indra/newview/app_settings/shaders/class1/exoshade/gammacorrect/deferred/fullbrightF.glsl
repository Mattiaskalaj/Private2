/** 
 * @file fullbrightF.glsl
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
out vec4 gl_FragColor;
#endif

VARYING vec4 vertex_color;
VARYING vec2 vary_texcoord0;

vec3 fullbrightAtmosTransport(vec3 light);
vec3 fullbrightScaleSoftClip(vec3 light);

void main() 
{
	float shadow = 1.0;

	vec4 color = diffuseLookup(vary_texcoord0.xy) * vertex_color;
	color.rgb = 0.755 * (color.rgb * color.rgb) + 0.245 * (color.rgb * color.rgb * color.rgb);
	
	color.rgb = fullbrightAtmosTransport(color.rgb);

	color.rgb = fullbrightScaleSoftClip(color.rgb);

	gl_FragColor = color;
}

