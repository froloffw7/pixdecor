/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 Scott Moreau <oreaus@gmail.com>
 * - Ported weston-smoke to compute shader set
 * Copyright (c) 2024 Ilia Bozhinov <ammen99@gmail.com>
 * - Awesome optimizations
 * Copyright (c) 2024 Andrew Pliatsikas <futurebytestore@gmail.com>
 * - Ported effect shaders to compute
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
// #define __DEBUG_SHADER__
// #define __DEBUG_SHADER_RENDER__

#include <wayfire/debug.hpp>
#include <wayfire/render.hpp>

#include "smoke-shaders.hpp"
#include "deco-theme.hpp"
#include "deco-shader.hpp"

namespace wf::pixdecor
{

static std::string stitch_smoke_shader(const std::string& source)
{
    return smoke_header + source + effect_run_for_region_main;
}

// ported from https://www.shadertoy.com/view/WdXBW4
static const char *render_source_clouds =
    R"(
#version 320 es
precision highp float;
precision highp image2D;

layout(binding = 0, rgba32f) uniform writeonly image2D out_tex;
layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
layout(location = 1) uniform int title_height;
layout(location = 2) uniform int border_size;
layout(location = 5) uniform int width;
layout(location = 6) uniform int height;
layout(location = 7) uniform int radius;
layout(location = 9) uniform float current_time;
float cloudscale=2.1;  // Added cloudscale parameter

const mat2 m = mat2(1.6, 1.2, -1.2, 1.6);

vec2 hash(vec2 p) {
    p = vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)));
    return -1.0 + 2.0 * fract(sin(p) * 43758.5453123);
}

float noise(vec2 p) {
    const float K1 = 0.366025404; // (sqrt(3)-1)/2;
    const float K2 = 0.211324865; // (3-sqrt(3))/6;
    vec2 i = floor(p + (p.x + p.y) * K1);
    vec2 a = p - i + (i.x + i.y) * K2;
    vec2 o = (a.x > a.y) ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
    vec2 b = a - o + K2;
    vec2 c = a - 1.0 + 2.0 * K2;
    vec3 h = max(0.5 - vec3(dot(a, a), dot(b, b), dot(c, c)), 0.0);
    vec3 n = h * h * h * h * vec3(dot(a, hash(i + 0.0)), dot(b, hash(i + o)), dot(c, hash(i + 1.0)));
    return dot(n, vec3(70.0));
}

float fbm(vec2 n) {
    float total = 0.0, amplitude = 0.1;
    for (int i = 0; i < 7; i++) {
        total += noise(n) * amplitude;
        n = m * n;
        amplitude *= 0.4;
    }
    return total;
}

void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);

    int x = pos.x;
    int y = pos.y;


      // Check if the pixel should be drawn
    if (x >= border_size && x <= (width - 1) - border_size && y >= title_height && y <= (height - 1) - border_size)
    {
        return;
    }

    vec2 uv = vec2(pos) / vec2(gl_NumWorkGroups.x * gl_WorkGroupSize.x, gl_NumWorkGroups.y * gl_WorkGroupSize.y);

    float time = 0.003 * current_time;  // Time variable for animation

    float cloudPattern1 = fbm(uv * 10.0 * cloudscale + time);
    float cloudPattern2 = fbm(uv * 5.0 * cloudscale + time);
    float cloudPattern3 = fbm(uv * 3.0 * cloudscale + time);

    // Combine different cloud patterns with different weights
    float cloudPattern = 0.5 * cloudPattern1 + 0.3 * cloudPattern2 + 0.2 * cloudPattern3;

    // Ridge noise shape
    float ridgeNoiseShape = 0.0;
    uv *= cloudscale * 1.1;  // Adjust scale
    float weight = 0.8;
    for (int i = 0; i < 8; i++) {
        ridgeNoiseShape += abs(weight * noise(uv));
        uv = m * uv + time;
        weight *= 0.7;
    }

    // Noise shape
    float noiseShape = 0.0;
    uv = vec2(pos) / vec2(gl_NumWorkGroups.x * gl_WorkGroupSize.x, gl_NumWorkGroups.y * gl_WorkGroupSize.y);
    uv *= cloudscale * 1.1;  // Adjust scale
    weight = 0.7;
    for (int i = 0; i < 8; i++) {
        noiseShape += weight * noise(uv);
        uv = m * uv + time;
        weight *= 0.6;
    }

    noiseShape *= ridgeNoiseShape + noiseShape;

    // Noise color
    float noiseColor = 0.0;
    uv = vec2(pos) / vec2(gl_NumWorkGroups.x * gl_WorkGroupSize.x, gl_NumWorkGroups.y * gl_WorkGroupSize.y);
    uv *= 2.0 * cloudscale;  // Adjust scale
    weight = 0.4;
    for (int i = 0; i < 7; i++) {
        noiseColor += weight * noise(uv);
        uv = m * uv + time;
        weight *= 0.6;
    }

    // Noise ridge color
    float noiseRidgeColor = 0.0;
    uv = vec2(pos) / vec2(gl_NumWorkGroups.x * gl_WorkGroupSize.x, gl_NumWorkGroups.y * gl_WorkGroupSize.y);
    uv *= 3.0 * cloudscale;  // Adjust scale
    weight = 0.4;
    for (int i = 0; i < 7; i++) {
        noiseRidgeColor += abs(weight * noise(uv));
        uv = m * uv + time;
        weight *= 0.6;
    }

    noiseColor += noiseRidgeColor;

    // Sky tint
    float skytint = 0.5;
    vec3 skyColour1 = vec3(0.1, 0.2, 0.3);
    vec3 skyColour2 = vec3(0.4, 0.7, 1.0);
    vec3 skycolour = mix(skyColour1, skyColour2, smoothstep(0.4, 0.6, uv.y));

    // Cloud darkness
    float clouddark = 0.5;

    // Cloud Cover, Cloud Alpha
    float cloudCover = 0.01;
    float cloudAlpha = 2.0;

    // Movement effect
    uv = uv + time;

    // Use a bright color for clouds
    vec3 cloudColor = vec3(1.0, 1.0, 1.0); ;  // Bright white color for clouds

    // Mix the cloud color with the background, considering darkness, cover, and alpha
    vec3 finalColor = mix(skycolour, cloudColor * clouddark, cloudPattern + noiseShape + noiseColor) * (1.0 - cloudCover) + cloudColor * cloudCover;
    finalColor = mix(skycolour, finalColor, cloudAlpha);

    imageStore(out_tex, pos, vec4(finalColor, 1.0));
}



)";

// ported from https://github.com/keijiro/ShaderSketches/blob/master/Fragment/Dots3.glsl
static const char *render_source_halftone =
    R"(
#version 310 es
precision highp float;
precision highp image2D;

layout(binding = 0, rgba32f) writeonly uniform highp image2D out_tex;
layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(location = 1) uniform int title_height;
layout(location = 2) uniform int border_size;
layout(location = 5) uniform int width;
layout(location = 6) uniform int height;
layout(location = 7) uniform int radius;
layout(location = 9) uniform float time;

const vec2 resolution = vec2(1280.0, 720.0);
const float timeFactor = 0.025;

float rand(vec2 uv) {
    return fract(sin(dot(uv, vec2(12.9898, 78.233))) * 43758.5453);
}

vec2 rotate(vec2 p, float theta) {
    vec2 sncs = vec2(sin(theta), cos(theta));
    return vec2(p.x * sncs.y - p.y * sncs.x, p.x * sncs.x + p.y * sncs.y);
}

float swirl(vec2 coord, float t) {
    float l = length(coord) / resolution.x;
    float phi = atan(coord.y, coord.x + 1e-6);
    return sin(l * 10.0 + phi - t * 4.0) * 0.5 + 0.5;
}

float halftone(vec2 coord, float angle, float t, float amp) {
    coord -= resolution * 0.5;
    float size = resolution.x / (60.0 + sin(time * timeFactor * 0.5) * 50.0);
    vec2 uv = rotate(coord / size, angle / 180.0 * 3.14);
    vec2 ip = floor(uv); // column, row
    vec2 odd = vec2(0.5 * mod(ip.y, 2.0), 0.0); // odd line offset
    vec2 cp = floor(uv - odd) + odd; // dot center
    float d = length(uv - cp - 0.5) * size; // distance
    float r = swirl(cp * size, t) * size * 0.5 * amp; // dot radius
    return 1.0 - clamp(d  - r, 0.0, 1.0);
}

void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);

       // Extract x and y coordinates
    int x = pos.x;
    int y = pos.y;


      // Check if the pixel should be drawn
    if (x >= border_size && x <= (width - 1) - border_size && y >= title_height && y <= (height - 1) - border_size)
    {
        return;
    }

    vec3 c1 = 1.0 - vec3(1.0, 0.0, 0.0) * halftone(vec2(pos),   0.0, time * timeFactor * 1.00, 0.7);
    vec3 c2 = 1.0 - vec3(0.0, 1.0, 0.0) * halftone(vec2(pos),  30.0, time * timeFactor * 1.33, 0.7);
    vec3 c3 = 1.0 - vec3(0.0, 0.0, 1.0) * halftone(vec2(pos), -30.0, time * timeFactor * 1.66, 0.7);
    vec3 c4 = 1.0 - vec3(1.0, 1.0, 1.0) * halftone(vec2(pos),  60.0, time * timeFactor * 2.13, 0.4);

    // Output the final color
    imageStore(out_tex, pos, vec4(c1 * c2 * c3 * c4, 1.0));
}
)";

// ported from https://www.shadertoy.com/view/WdjGRc
static const char *render_source_lava =
    R"(
#version 320 es
precision highp float;
precision highp image2D;

layout(binding = 0, rgba32f) uniform readonly image2D in_tex;
layout(binding = 0, rgba32f) uniform writeonly image2D out_tex;
layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;


layout(location = 1) uniform int title_height;
layout(location = 2) uniform int border_size;
layout(location = 5) uniform int width;
layout(location = 6) uniform int height;
layout(location = 7) uniform int radius;
layout(location = 9) uniform float current_time;

vec3 effect(float speed, vec2 uv, float time, float scale) {
    float t = mod(time * 0.005, 6.0);
    float rt = 0.00000000000001 * sin(t * 0.45);

    mat2 m1 = mat2(cos(rt), -sin(rt), -sin(rt), cos(rt));
    vec2 uva = uv * m1 * scale;
    float irt = 0.005 * cos(t * 0.05);
    mat2 m2 = mat2(sin(irt), cos(irt), -cos(irt), sin(irt));

    for (int i = 1; i < 40; i += 1) {
        float it = float(i);
        uva *= m2;
        uva.y += -1.0 + (0.6 / it) * cos(t + it * uva.x + 0.5 * it) * float(mod(it, 0.5) == 0.0);
        uva.x += 1.0 + (0.5 / it) * cos(t + it * uva.y * 0.1 / 5.0 + 0.5 * (it + 15.0));  // Adjust the scaling factor for y-coordinate
    }

    float n = 0.5;
    float r = n + n * sin(4.0 * uva.x + t);
    float gb = n + n * sin(3.0 * uva.y);
    return vec3(r, gb * 0.8 * r, gb * r);
}

void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);

vec2 uv = vec2(pos) / vec2(1000, 2000);
    uv = 2.0 * uv - 1.0;
    uv *= (10.3 + 0.1 * sin(current_time * 0.01));

       // Extract x and y coordinates
    int x = pos.x;
    int y = pos.y;

    // Check if the pixel should be drawn
    if (x >= border_size && x <= (width - 1) - border_size && y >= title_height && y <= (height - 1) - border_size)
    {
        return;
    }


   // uv.y -= current_time * 0.013;
//uv.x -= current_time * 0.026;
    vec3 col = effect(0.001, uv, current_time, 0.5);
    // col += effect(0.5, uv * 3.0, 2.0 * current_time + 10.0, 0.5) * 0.3;
    // col += effect(0.5, sin(current_time * 0.01) * uv * 2.0, 2.0 * current_time + 10.0, 0.5) * 0.1;

    // Output the final color to the output texture
    imageStore(out_tex, pos, vec4(col, 1.0));
}
)";

// ported from https://github.com/keijiro/ShaderSketches/blob/master/Fragment/Eyes2.glsl
static const char *render_source_pattern =
    R"(
#version 310 es
precision highp float;
precision highp image2D;

layout(binding = 0, rgba32f) writeonly uniform highp image2D out_tex;
layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;


layout(location = 1) uniform int title_height;
layout(location = 2) uniform int border_size;
layout(location = 5) uniform int width;
layout(location = 6) uniform int height;
layout(location = 7) uniform int radius;
layout(location = 9) uniform float time;

float rand(vec2 uv)
{
    return fract(sin(dot(uv, vec2(12.9898, 78.233))) * 43758.5453);
}

vec3 hue2rgb(float h)
{
    h = fract(h) * 6.0 - 2.0;
    return clamp(vec3(abs(h - 1.0) - 1.0, 2.0 - abs(h), 2.0 - abs(h - 2.0)), 0.0, 1.0);
}

vec3 eyes(vec2 coord, vec2 resolution)
{
    const float pi = 3.141592;
    float t = 0.4 * time * 0.05;
    float aspectRatio = resolution.x / resolution.y;
    float div = 20.0;
    float sc = 30.0;

    vec2 p = (coord - resolution / 2.0) / sc - 0.5;

    // center offset
    float dir = floor(rand(floor(p) + floor(t) * 0.11) * 4.0) * pi / 2.0;
    vec2 offs = vec2(sin(dir), cos(dir)) * 0.6;
    offs *= smoothstep(0.0, 0.1, fract(t));
    offs *= smoothstep(0.4, 0.5, 1.0 - fract(t));

    // circles
    float l = length(fract(p) + offs - 0.5);
    float rep = sin((rand(floor(p)) * 2.0 + 2.0) * t) * 4.0 + 5.0;
    float c = (abs(0.5 - fract(l * rep + 0.5)) - 0.25) * sc / rep;

    // grid lines
    vec2 gr = (abs(0.5 - fract(p + 0.5)) - 0.05) * sc;
    c = clamp(min(min(c, gr.x), gr.y), 0.0, 1.0);

    return hue2rgb(rand(floor(p) * 0.3231)) * c;
}

void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    vec2 resolution = vec2(float(width), float(height));

    int x = pos.x;
    int y = pos.y;

    // Check if the pixel should be drawn
    if (x >= border_size && x <= (width - 1) - border_size && y >= title_height && y <= (height - 1) - border_size)
    {
        return;
    }

    vec3 color = eyes(vec2(pos), resolution);

    // Output the final color
    imageStore(out_tex, pos, vec4(color, 1.0));
}


)";

// original (by phodius)
static const char *render_source_hex =
    R"(
#version 310 es
precision highp float;
precision highp int;

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
layout(binding = 0, rgba32f) writeonly uniform highp image2D OutputImage;


layout(location = 1) uniform int title_height;
layout(location = 2) uniform int border_size;
layout(location = 5) uniform int width;
layout(location = 6) uniform int height;
layout(location = 7) uniform int radius;
layout(location = 9) uniform float iTime;
vec2 iResolution;

float rand(vec2 co) {
    return fract(sin(dot(co.xy, vec2(12.9898, 4.1414))) * 43758.5453);
}

// Hexagon function
vec4 hexagon(vec2 p)
{
    vec2 q = vec2(p.x * 2.0 * 0.5773503, p.y + p.x * 0.5773503);

    vec2 pi = floor(q);
    vec2 pf = fract(q);

    float v = mod(pi.x * 9.0, 0.0);

    float ca = step(1.0, v);
    float cb = step(2.0, v);
    vec2 ma = step(pf.xy, pf.yx);

    float e = 0.0;
    float f = length((fract(p) - 10.5) * vec2(1.0, 0.85));

    return vec4(pi + ca - cb * ma, e, f);
}
void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);

    int x = pos.x;
    int y = pos.y;

    // Check if the pixel should be drawn
    if (x >= border_size && x <= (width - 1) - border_size && y >= title_height && y <= (height - 1) - border_size)
    {
        return;
    }

    vec2 uv = (vec2(pos) + 0.5) / float(width);

    // Generate a random value
    float randVal = rand(uv);

    // Apply hexagon logic
    vec2 p = (-float(width) + 2.0 * vec2(pos)) / float(height);
    vec4 h = hexagon(40.0 * p + vec2(0.5 * iTime * 0.125));

    float col = 0.01 + 0.15 * rand(vec2(h.xy)) * 1.0;
    col *= 4.3 + 0.15 * sin(10.0 * h.z);

    // Use hardcoded colors for shades with gradient
    vec3 shadeColor1 = vec3(0.1, 0.1, 0.1) + 1.1 * col * h.z;   // Dark gray with gradient
    vec3 shadeColor2 = vec3(0.4, 0.4, 0.4) + 1.1 * col;  // Gray with gradient
    vec3 shadeColor3 = vec3(0.9, 0.9, 0.9) + 0.1 * col;  // Light gray with gradient

    // Use different hardcoded colors based on the random value
    vec3 finalColor = mix(shadeColor1, mix(shadeColor2, shadeColor3, randVal), col);

    // Use imageStore instead of writing to buffer
    imageStore(OutputImage, pos, vec4(finalColor, 1.0));
}
)";

// ported from https://github.com/keijiro/ShaderSketches/blob/master/Fragment/Zebra.glsl
static const char *render_source_zebra =
    R"(
#version 310 es
precision highp float;
precision highp image2D;

layout(binding = 0, rgba32f) writeonly uniform highp image2D out_tex;
layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(location = 1) uniform int title_height;
layout(location = 2) uniform int border_size;
layout(location = 5) uniform int width;
layout(location = 6) uniform int height;
layout(location = 7) uniform int radius;
layout(location = 9) uniform float time;
const float resolutionY = 720.0; // Set to constant resolution of 720p
const float pi = 3.14159265359;
const float timeFactor = 0.05; // Adjust this factor to control the speed of the animation

float rand(vec2 uv)
{
    return fract(sin(dot(uv, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);

    int x = pos.x;
    int y = pos.y;

    // Check if the pixel should be drawn
    if (x >= border_size && x <= (width - 1) - border_size && y >= title_height && y <= (height - 1) - border_size)
    {
        return;
    }
    float size = resolutionY / 10.0; // cell size in pixel

    vec2 p1 = vec2(pos) / size; // normalized pos
    vec2 p2 = fract(p1) - 0.5; // relative pos from cell center

    // random number
    float rnd = dot(floor(p1), vec2(12.9898, 78.233));
    rnd = fract(sin(rnd) * 43758.5453);

    // rotation matrix
    float phi = rnd * pi * 2.0 + time * 0.4 * timeFactor;
    mat2 rot = mat2(cos(phi), -sin(phi), sin(phi), cos(phi));

    vec2 p3 = rot * p2; // apply rotation
    p3.y += sin(p3.x * 5.0 + time * 2.0 * timeFactor) * 0.12; // wave

    float rep = fract(rnd * 13.285) * 8.0 + 2.0; // line repetition
    float gr = fract(p3.y * rep + time * 0.8 * timeFactor); // repeating gradient

    // make antialiased line by saturating the gradient
    float c = clamp((0.25 - abs(0.5 - gr)) * size * 0.75 / rep, 0.0, 1.0);
    c *= max(0.0, 1.0 - length(p2) * 0.6); // darken corners

    vec2 bd = (0.5 - abs(p2)) * size - 2.0; // border lines
    c *= clamp(min(bd.x, bd.y), 0.0, 1.0);

    // Output the final color
    imageStore(out_tex, pos, vec4(c, c, c, 1.0));
}


)";

// ported from https://www.shadertoy.com/view/dlGfWV
static const char *render_source_neural_network =
    R"(

#version 310 es
precision highp float;
precision highp image2D;

//layout(binding = 0, rgba32f) readonly uniform highp image2D neural_network_texture;  // Use binding point 0
layout(binding = 0, rgba32f) writeonly uniform highp image2D out_tex;  // Use binding point 1

//layout(binding = 0, rgba32f) writeonly uniform highp image2D out_tex;

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(location = 1) uniform int title_height;
layout(location = 2) uniform int border_size;
layout(location = 5) uniform int width;
layout(location = 6) uniform int height;
layout(location = 7) uniform int radius;
layout(location = 9) uniform float time;

uniform vec2 iResolution;
uniform float iTime;

mat2 rotate2D(float r) {
    float c = cos(r);
    float s = sin(r);
    return mat2(c, -s, s, c);
}

void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);

    int x = pos.x;
    int y = pos.y;

    // Check if the pixel should be drawn
    if (x >= border_size && x <= (width - 1) - border_size && y >= title_height && y <= (height - 1) - border_size)
    {
        return;
    }

    // Normalized pixel coordinates (from 0 to 1)
    vec2 uv = (vec2(pos) - 0.5 * float(gl_NumWorkGroups.x)) / float(gl_WorkGroupSize.x);

    // Scale the uv coordinates by a factor of 8
    uv *= 0.05;

    vec3 col = vec3(0);
    float t = 0.05 * time;

    vec2 n = vec2(0), q;
    vec2 N = vec2(0);
    vec2 p = uv + sin(t * 0.1) / 10.0;
    float S = 10.0;
    mat2 m = rotate2D(1.0);

    for (float j = 0.0; j < 30.0; j++) {
        p *= m;
        n *= m;
        q = p * S + j + n + t;
        n += sin(q);
        N += cos(q) / S;
        S *= 1.2;
    }

    col = vec3(1, 2, 4) * pow((N.x + N.y + 0.2) + 0.005 / length(N), 2.1);

    imageStore(out_tex, pos, vec4(col, 1.0));
}
)";

// Ported from https://www.shadertoy.com/view/llSyDh
static const char *render_source_hexagon_maze =
    R"(
#version 320 es

precision highp float;
precision highp int;
precision highp image2D;

//layout (location = 0) out vec4 fragColor;
layout (binding = 0, rgba32f) writeonly uniform image2D fragColor;

layout(location = 1) uniform int title_height;
layout(location = 2) uniform int border_size;
layout(location = 5) uniform int width;
layout(location = 6) uniform int height;
layout(location = 7) uniform int radius;
layout(location = 9) uniform float iTime;
uniform vec2 iResolution;
layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;


// Interlaced variation - Interesting, but patched together in a hurry.
//#define INTERLACING

// A quick hack to get rid of the winding overlay - in order to show the maze only.
//#define MAZE_ONLY



// Helper vector. If you're doing anything that involves regular triangles or hexagons, the
// 30-60-90 triangle will be involved in some way, which has sides of 1, sqrt(3) and 2.
const vec2 s = vec2(1, 1.7320508);

// Standard vec2 to float hash - Based on IQ's original.
float hash21(vec2 p){ return fract(sin(dot(p, vec2(141.173, 289.927)))*43758.5453); }


// Standard 2D rotation formula.
mat2 r2(in float a){ float c = cos(a), s = sin(a); return mat2(c, -s, s, c); }


// The 2D hexagonal isosuface function: If you were to render a horizontal line and one that
// slopes at 60 degrees, mirror, then combine them, you'd arrive at the following.
float hex(in vec2 p){

    p = abs(p);

    // Below is equivalent to:
    //return max(p.x*.5 + p.y*.866025, p.x);

    return max(dot(p, s*.5), p.x); // Hexagon.

}

// This function returns the hexagonal grid coordinate for the grid cell, and the corresponding
// hexagon cell ID - in the form of the central hexagonal point. That's basically all you need to
// produce a hexagonal grid.
//
// When working with 2D, I guess it's not that important to streamline this particular function.
// However, if you need to raymarch a hexagonal grid, the number of operations tend to matter.
// This one has minimal setup, one "floor" call, a couple of "dot" calls, a ternary operator, etc.
// To use it to raymarch, you'd have to double up on everything - in order to deal with
// overlapping fields from neighboring cells, so the fewer operations the better.
vec4 getHex(vec2 p){

    // The hexagon centers: Two sets of repeat hexagons are required to fill in the space, and
    // the two sets are stored in a "vec4" in order to group some calculations together. The hexagon
    // center we'll eventually use will depend upon which is closest to the current point. Since
    // the central hexagon point is unique, it doubles as the unique hexagon ID.
    vec4 hC = floor(vec4(p, p - vec2(.5, 1))/s.xyxy) + .5;

    // Centering the coordinates with the hexagon centers above.
    vec4 h = vec4(p - hC.xy*s, p - (hC.zw + .5)*s);

    // Nearest hexagon center (with respect to p) to the current point. In other words, when
    // "h.xy" is zero, we're at the center. We're also returning the corresponding hexagon ID -
    // in the form of the hexagonal central point. Note that a random constant has been added to
    // "hC.zw" to further distinguish it from "hC.xy."
    //
    // On a side note, I sometimes compare hex distances, but I noticed that Iomateron compared
    // the Euclidian version, which seems neater, so I've adopted that.
    return dot(h.xy, h.xy)<dot(h.zw, h.zw) ? vec4(h.xy, hC.xy) : vec4(h.zw, hC.zw + vec2(.5, 1));

}

// Dot pattern.
float dots(in vec2 p){

    p = abs(fract(p) - .5);

    return length(p); // Circles.

    //return (p.x + p.y)/1.5 + .035; // Diamonds.

    //return max(p.x, p.y) + .03; // Squares.

    //return max(p.x*.866025 + p.y*.5, p.y) + .01; // Hexagons.

    //return min((p.x + p.y)*.7071, max(p.x, p.y)) + .08; // Stars.


}


// Distance field for the arcs. I think it's called poloidal rotation, or something like that.
float dfPol(vec2 p){

    return length(p); // Circular arc.

    // There's no rule that says the arcs have to be rounded. Here's a hexagonal one.
    //return hex(p);

    // Dodecahedron.
    //return max(hex(p), hex(r2(3.14159/6.)*p));

    // Triangle.
    //return max(abs(p.x)*.866025 - p.y, p.y);

}


// Truchet pattern distance field.
float df(vec2 p, float dir){

    // Weird UV coordinates. The first entry is the Truchet distance field itself,
    // and the second is the polar angle of the arc pixel. The extra ".1" is just a bit
    // of mutational scaling, or something... I can't actually remember why it's there. :)
    vec2 uv = vec2(p.x + .1, p.y);//*vec2(1, 1); // Scaling.

    // A checkered dot pattern. At present the pattern needs to have flip symmetry about
    // the center, but I'm pretty sure regular textures could be applied with a few
    // minor changes. Due to the triangular nature of the Truchet pattern, factors of "3"
    // were necessary, but factors of "1.5" seemed to work too. Hence the "4.5."
    return min(dots(uv*4.5), dots(uv*4.5 + .5)) - .3;

}


// Polar coordinate of the arc pixel.
float getPolarCoord(vec2 q, float dir){

    // The actual animation. You perform that before polar conversion.
    q = r2((iTime*0.05)*dir)*q;

    // Polar angle.
    const float aNum = 1.;
    float a = atan(q.y, q.x);

    // Wrapping the polar angle.
    return mod(a/3.14159, 2./aNum) - 1./aNum;


}



void main() {
    vec2 iResolution = vec2(width, height);
    ivec2 id = ivec2(gl_GlobalInvocationID.xy);
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    int x = pos.x;
    int y = pos.y;

    // Check if the pixel should be drawn
    if (x >= border_size && x <= (width - 1) - border_size && y >= title_height && y <= (height - 1) - border_size)
    {
        return;
    }
    vec2 fragCoord = vec2(id);
    float res = clamp(iResolution.y, 300.0, 600.0);
    vec2 u = (fragCoord - iResolution.xy*.5)/res;
    vec2 sc = u*4. + s.yx*(iTime*0.05)/12.;
    vec4 h = getHex(sc);
    vec4 h2 = getHex(sc - 1.0 / s);
    vec4 h3 = getHex(sc + 1.0 / s);
    vec2 p = h.xy;
    float eDist = hex(p);
    float cDist = dot(p, p);
    float rnd = hash21(h.zw);
 //float aRnd = sin(rnd*6.283 + (iTime*0.05)*1.5)*.5 + .5; // Animating the random number.

    #ifdef INTERLACING
    // Random vec3 - used for some overlapping.
    //vec3 lRnd = vec3(rnd*14.4 + .81, fract(rnd*21.3 + .97), fract(rnd*7.2 + .63));
    vec3 lRnd = vec3(hash21(h.zw + .23), hash21(h.zw + .96), hash21(h.zw + .47));
    #endif

    // It's possible to control the randomness to form some kind of repeat pattern.
    //rnd = mod(h.z + h.w, 2.);


    // Redundant here, but I might need it later.
    float dir = 1.;



    // Storage vector.
    vec2 q;


    // If the grid cell's random ID is above a threshold, flip the Y-coordinates.
    if(rnd>.5) p.y = -p.y;



    // Determining the closest of the three arcs to the current point, the keeping a copy
    // of the vector used to produce it. That way, you'll know just to render that particular
    // decorated arc, lines, etc - instead of all three.
    const float r = 1.;
    const float th = .2; // Arc thickness.

    // Arc one.
    q = p - vec2(0, r)/s;
    vec3 da = vec3(q, dfPol(q));

    // Arc two. "r2" could be hardcoded, but this is a relatively cheap 2D example.
    q = r2(3.14159*2./3.)*p - vec2(0, r)/s;
    vec3 db = vec3(q, dfPol(q));

     // Arc three.
    q = r2(3.14159*4./3.)*p - vec2(0, r)/s;
    vec3 dc = vec3(q, dfPol(q));

    // Compare distance fields, and return the vector used to produce the closest one.
    vec3 q3 = da.z<db.z && da.z<dc.z? da : db.z<dc.z ? db : dc;


    // TRUCHET PATTERN
    //
    // Set the poloidal arc radius: You can change the poloidal distance field in
    // the "dfPol" function to a different curve shape, but you'll need to change
    // the radius to one of the figures below.
    //
    q3.z -= .57735/2. + th/2.;  // Circular and dodecahedral arc/curves.
    //q3.z -= .5/2. + th/2.;  // Hexagon curve.
    //q3.z -= .7071/2. + th/2.;  // Triangle curve.

    q3.z = max(q3.z, -th - q3.z); // Chop out the smaller radius. The result is an arc.

    // Store the result in "d" - only to save writing "q3.z" everywhere.
    float d = q3.z;

    // If you'd like to see the maze by itself.
    #ifdef MAZE_ONLY
    d += 1e5;
    #endif

    // Truchet border.
    float dBord = max(d - .015, -d);





    // MAZE BORDERS
    // Producing the stright-line arc borders. Basically, we're rendering some hexagonal borders around
    // the arcs. The result is the hexagonal maze surrounding the Truchet pattern.
    q = q3.xy;
    const float lnTh = .05;
    q = abs(q);

    float arcBord = hex(q);
    //float arcBord = length(q); // Change arc length to ".57735."
    //float arcBord = max(hex(q), hex(r2(3.14159/6.)*q)); // Change arc length to ".57735."

    // Making the hexagonal arc.
    float lnOuter = max(arcBord - .5, -(arcBord - .5 + lnTh)); //.57735


    #ifdef INTERLACING
    float ln = min(lnOuter, (q.y*.866025 + q.x*.5, q.x) - lnTh);
    #else
    float ln = min(lnOuter, arcBord - lnTh);
    #endif
    float lnBord = ln - .03; // Border lines to the maze border, if that makes any sense. :)


    ///////
    // The moving Truchet pattern. The polar coordinates consist of a wrapped angular coordinate,
    // and the distance field itself.
    float a = getPolarCoord(q3.xy, dir);
    float d2 = df(vec2(q3.z, a), dir);
    float dMask = smoothstep(0., .015, d);
    vec3 bg =  mix(vec3(0, .0, .6), vec3(0, .9, .0), dot(sin(u*6. - cos(u*3.)), vec2(.4/2.)) + .4);



    bg = mix(bg, bg.xzy, dot(sin(u*6. - cos(u*3.)), vec2(.4/2.)) + .4);
    bg = mix(bg, bg.zxy, dot(sin(u*3. + cos(u*3.)), vec2(.1/2.)) + .1);

    #ifdef INTERLACING
    // Putting in background cube lines for the interlaced version.
    float hLines = smoothstep(0., .02, eDist - .5 + .02);
    bg = mix(bg, vec3(0), smoothstep(0., .02, ln)*dMask*hLines);
    #endif

    // Lines over the maze lines. Applying difference logic, depending on whether the
    // pattern is interlaced or not.
    const float tr = 1.;

    float eDist2 = hex(h2.xy);
    float hLines2 = smoothstep(0., .02, eDist2 - .5 + .02);
    #ifdef INTERLACING
    if(rnd>.5 && lRnd.x<.5) hLines2 *= smoothstep(0., .02, ln);
    if(lRnd.x>.5) hLines2 *= dMask;
    #else
    if(rnd>.5) hLines2 *= smoothstep(0., .02, ln);
    hLines2 *= dMask;
    #endif
    bg = mix(bg, vec3(0), hLines2*tr);

    float eDist3 = hex(h3.xy);
    float hLines3 = smoothstep(0., .02, eDist3 - .5 + .02);
    #ifdef INTERLACING
    if(rnd<=.5 && lRnd.x>.5) hLines3 *= smoothstep(0., .02, ln);
    if(lRnd.x>.5) hLines3 *= dMask;
    #else
    if(rnd<=.5) hLines3 *= smoothstep(0., .02, ln);
    hLines3 *= dMask;
    #endif
    bg = mix(bg, vec3(0), hLines3*tr);


    // Using the two off-centered hex coordinates to give the background a bit of highlighting.
    float shade = max(1.25 - dot(h2.xy, h2.xy)*2., 0.);
    shade = min(shade, max(dot(h3.xy, h3.xy)*3. + .25, 0.));
    bg = mix(bg, vec3(0), (1.-shade)*.5);

    // I wanted to change the colors of everything at the last minute. It's pretty hacky, so
    // when I'm feeling less lazy, I'll tidy it up. :)
    vec3 dotCol = bg.zyx*vec3(1.5, .4, .4);
    vec3 bCol = mix(bg.zyx, bg.yyy, .25);
    bg = mix(bg.yyy, bg.zyx, .25);



    // Under the random threshold, and we draw the lines under the Truchet pattern.
    #ifdef INTERLACING
    if(lRnd.x>.5){
       bg = mix(bg, vec3(0), (1. - smoothstep(0., .015, lnBord)));
       bg = mix(bg, bCol, (1. - smoothstep(0., .015, ln)));
       // Center lines.
       bg = mix(bg, vec3(0), smoothstep(0., .02, eDist3 - .5 + .02)*tr);
    }
    #else
    bg = mix(bg, vec3(0), (1. - smoothstep(0., .015, lnBord)));
    bg = mix(bg, bCol, (1. - smoothstep(0., .015, ln)));
    #endif



     // Apply the Truchet shadow to the background.
    bg = mix(bg, vec3(0), (1. - smoothstep(0., .07, d))*.5);


    // Place the Truchet field to the background, with some additional shading to give it a
    // slightly rounded, raised feel.
    //vec3 col = mix(bg, vec3(1)*max(-d*3. + .7, 0.), (1. - dMask)*.65);
    // Huttarl suggest slightly more shading on the snake-like pattern edges, so I added just a touch.
    vec3 col = mix(bg, vec3(1)*max(-d*9. + .4, 0.), (1. - dMask)*.65);




    // Apply the moving dot pattern to the Truchet.
    //dotCol = mix(dotCol, dotCol.xzy, dot(sin(u*3.14159*2. - cos(u.yx*3.14159*2.)*3.14159), vec2(.25)) + .5);
    col = mix(col, vec3(0), (1. - dMask)*(1. - smoothstep(0., .02, d2)));
    col = mix(col, dotCol, (1. - dMask)*(1. - smoothstep(0., .02, d2 + .125)));

    // Truchet border.
    col = mix(col, vec3(0), 1. - smoothstep(0., .015, dBord));

    #ifdef INTERLACING
    // Over the random threshold, and we draw the lines over the Truchet.
    if(lRnd.x<=.5){
        col = mix(col, vec3(0), (1. - smoothstep(0., .015, lnBord)));
        col = mix(col, bCol, (1. - smoothstep(0., .015, ln)));
        // Center lines.
        col = mix(col, vec3(0), smoothstep(0., .02, eDist2 - .5 + .02)*tr);
    }
    #endif




    // Subtle vignette.
    u = fragCoord/iResolution.xy;
    col *= pow(16.*u.x*u.y*(1. - u.x)*(1. - u.y) , .125) + .25;
    // Colored variation.
    //col = mix(pow(min(vec3(1.5, 1, 1)*col, 1.), vec3(1, 3, 16)), col,
            //pow(16.*u.x*u.y*(1. - u.x)*(1. - u.y) , .25)*.75 + .25);







    // Using the edge distance to produce some repeat contour lines. Standard stuff.
    //if (rnd>.5) h.xy = -h.yx;
    //float cont = clamp(cos(hex(h.xy)*6.283*12.)*1.5 + 1.25, 0., 1.);
    //col = mix(col, vec3(0), (1. - smoothstep(0., .015, ln))*(smoothstep(0., .015, d))*(1.-cont)*.5);



    // Very basic hatch line effect.
    float gr = dot(col, vec3(.299, .587, .114));
    float hatch = (gr<.45)? clamp(sin((sc.x - sc.y)*3.14159*40.)*2. + 1.5, 0., 1.) : 1.;
    float hatch2 = (gr<.25)? clamp(sin((sc.x + sc.y)*3.14159*40.)*2. + 1.5, 0., 1.) : 1.;

    col *= min(hatch, hatch2)*.5 + .5;
    col *= clamp(sin((sc.x - sc.y)*3.14159*80.)*1.5 + .75, 0., 1.)*.25 + 1.;



    vec2 u2 = fragCoord / iResolution.xy;
    col *= pow(16.0 * u2.x * u2.y * (1.0 - u2.x) * (1.0 - u2.y), 0.125) + 0.25;

    imageStore(fragColor, id, vec4(sqrt(max(col, 0.0)), 1.0));
}



)";

// ported from https://www.shadertoy.com/view/4td3zj
static const char *render_source_raymarched_truchet =
    R"(
#version 310 es

precision highp float;
precision highp int;
precision highp image2D;

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
layout(binding = 0, rgba8) writeonly uniform image2D imageOut;

layout(location = 1) uniform int title_height;
layout(location = 2) uniform int border_size;
layout(location = 5) uniform int width;
layout(location = 6) uniform int height;
layout(location = 7) uniform int radius;
layout(location = 9) uniform float  iTime;
uniform vec2 iResolution;

float heightMap(in vec2 p) {

    p *= 5.;

    // Hexagonal coordinates.
    vec2 h = vec2(p.x + p.y*.57735, p.y*1.1547);

    // Closest hexagon center.
    vec2 f = fract(h); h -= f;
    float c = fract((h.x + h.y)/3.);
    h =  c<.666 ?   c<.333 ?  h  :  h + 1.  :  h  + step(f.yx, f);

    p -= vec2(h.x - h.y*.5, h.y*.8660254);

    // Rotate (flip, in this case) random hexagons. Otherwise, you'd have a bunch of circles only.
    // Note that "h" is unique to each hexagon, so we can use it as the random ID.
    c = fract(cos(dot(h, vec2(41, 289)))*43758.5453); // Reusing "c."
    p -= p*step(c, .5)*2.; // Equivalent to: if (c<.5) p *= -1.;

    // Minimum squared distance to neighbors. Taking the square root after comparing, for speed.
    // Three partitions need to be checked due to the flipping process.
    p -= vec2(-1, 0);
    c = dot(p, p); // Reusing "c" again.
    p -= vec2(1.5, .8660254);
    c = min(c, dot(p, p));
    p -= vec2(0, -1.73205);
    c = min(c, dot(p, p));

    return sqrt(c);

    // Wrapping the values - or folding the values over (abs(c-.5)*2., cos(c*6.283*1.), etc) - to produce
    // the nicely lined-up, wavy patterns. I"m perfoming this step in the "map" function. It has to do
    // with coloring and so forth.
    //c = sqrt(c);
    //c = cos(c*6.283*1.) + cos(c*6.283*2.);
    //return (clamp(c*.6+.5, 0., 1.));

}

// Raymarching an XY-plane - raised a little by the hexagonal Truchet heightmap. Pretty standard.
float map(vec3 p){


    float c = heightMap(p.xy); // Height map.
    // Wrapping, or folding the height map values over, to produce the nicely lined-up, wavy patterns.
    c = cos(c*6.283*1.) + cos(c*6.283*2.);
    c = (clamp(c*.6+.5, 0., 1.));


    // Back plane, placed at vec3(0., 0., 1.), with plane normal vec3(0., 0., -1).
    // Adding some height to the plane from the heightmap. Not much else to it.
    return 1. - p.z - c*.025;


}

// The normal function with some edge detection and curvature rolled into it. Sometimes, it's possible to
// get away with six taps, but we need a bit of epsilon value variance here, so there's an extra six.
vec3 getNormal(vec3 p, inout float edge, inout float crv) {

    vec2 e = vec2(.01, 0); // Larger epsilon for greater sample spread, thus thicker edges.

    // Take some distance function measurements from either side of the hit point on all three axes.
    float d1 = map(p + e.xyy), d2 = map(p - e.xyy);
    float d3 = map(p + e.yxy), d4 = map(p - e.yxy);
    float d5 = map(p + e.yyx), d6 = map(p - e.yyx);
    float d = map(p)*2.;    // The hit point itself - Doubled to cut down on calculations. See below.

    // Edges - Take a geometry measurement from either side of the hit point. Average them, then see how
    // much the value differs from the hit point itself. Do this for X, Y and Z directions. Here, the sum
    // is used for the overall difference, but there are other ways. Note that it's mainly sharp surface
    // curves that register a discernible difference.
    edge = abs(d1 + d2 - d) + abs(d3 + d4 - d) + abs(d5 + d6 - d);
    //edge = max(max(abs(d1 + d2 - d), abs(d3 + d4 - d)), abs(d5 + d6 - d)); // Etc.

    // Once you have an edge value, it needs to normalized, and smoothed if possible. How you
    // do that is up to you. This is what I came up with for now, but I might tweak it later.
    edge = smoothstep(0., 1., sqrt(edge/e.x*2.));

    // We may as well use the six measurements to obtain a rough curvature value while we're at it.
    crv = clamp((d1 + d2 + d3 + d4 + d5 + d6 - d*3.)*32. + .6, 0., 1.);

    // Redoing the calculations for the normal with a more precise epsilon value.
    e = vec2(.0025, 0);
    d1 = map(p + e.xyy), d2 = map(p - e.xyy);
    d3 = map(p + e.yxy), d4 = map(p - e.yxy);
    d5 = map(p + e.yyx), d6 = map(p - e.yyx);


    // Return the normal.
    // Standard, normalized gradient mearsurement.
    return normalize(vec3(d1 - d2, d3 - d4, d5 - d6));
}



// I keep a collection of occlusion routines... OK, that sounded really nerdy. :)
// Anyway, I like this one. I'm assuming it's based on IQ's original.
float calculateAO(in vec3 p, in vec3 n)
{
    float sca = 2., occ = 0.;
    for(float i=0.; i<5.; i++){

        float hr = .01 + i*.5/4.;
        float dd = map(n * hr + p);
        occ += (hr - dd)*sca;
        sca *= 0.7;
    }
    return clamp(1.0 - occ, 0., 1.);
}


/*
// Surface bump function. Cheap, but with decent visual impact.
float bumpSurf3D( in vec3 p){

    float c = heightMap((p.xy + p.z*.025)*6.);
    c = cos(c*6.283*3.);
    //c = sqrt(clamp(c+.5, 0., 1.));
    c = (c*.5 + .5);

    return c;

}

// Standard function-based bump mapping function.
vec3 dbF(in vec3 p, in vec3 nor, float bumpfactor){

    const vec2 e = vec2(0.001, 0);
    float ref = bumpSurf3D(p);
    vec3 grad = (vec3(bumpSurf3D(p - e.xyy),
                      bumpSurf3D(p - e.yxy),
                      bumpSurf3D(p - e.yyx) )-ref)/e.x;

    grad -= nor*dot(nor, grad);

    return normalize( nor + grad*bumpfactor );

}
*/

// Compact, self-contained version of IQ's 3D value noise function.
float n3D(vec3 p){

    const vec3 s = vec3(7, 157, 113);
    vec3 ip = floor(p); p -= ip;
    vec4 h = vec4(0., s.yz, s.y + s.z) + dot(ip, s);
    p = p*p*(3. - 2.*p); //p *= p*p*(p*(p * 6. - 15.) + 10.);
    h = mix(fract(sin(h)*43758.5453), fract(sin(h + s.x)*43758.5453), p.x);
    h.xy = mix(h.xz, h.yw, p.y);
    return mix(h.x, h.y, p.z); // Range: [0, 1].
}

// Simple environment mapping. Pass the reflected vector in and create some
// colored noise with it. The normal is redundant here, but it can be used
// to pass into a 3D texture mapping function to produce some interesting
// environmental reflections.
vec3 envMap(vec3 rd, vec3 sn){

    vec3 sRd = rd; // Save rd, just for some mixing at the end.

    // Add a time component, scale, then pass into the noise function.
    rd.xy -= iTime*.25 *0.0125;
    rd *= 3.;

    float c = n3D(rd)*.57 + n3D(rd*2.)*.28 + n3D(rd*4.)*.15; // Noise value.
    c = smoothstep(0.4, 1., c); // Darken and add contast for more of a spotlight look.

    vec3 col = vec3(c, c*c, c*c*c*c); // Simple, warm coloring.
    //vec3 col = vec3(min(c*1.5, 1.), pow(c, 2.5), pow(c, 12.)); // More color.

    // Mix in some more red to tone it down and return.
    return mix(col, col.yzx, sRd*.25+.25);

}



// vec2 to vec2 hash.
vec2 hash22(vec2 p) {

    // Faster, but doesn't disperse things quite as nicely as other combinations. :)
    float n = sin(dot(p, vec2(41, 289)));
    return fract(vec2(262144, 32768)*n)*.75 + .25;

    // Animated.
    //p = fract(vec2(262144, 32768)*n);
    //return sin( p*6.2831853 + iTime *0.0125)*.35 + .65;

}

// 2D 2nd-order Voronoi: Obviously, this is just a rehash of IQ's original. I've tidied
// up those if-statements. Since there's less writing, it should go faster. That's how
// it works, right? :)
//
float Voronoi(in vec2 p){

    vec2 g = floor(p), o; p -= g;

    vec3 d = vec3(1); // 1.4, etc. "d.z" holds the distance comparison value.

    for(int y = -1; y <= 1; y++){
        for(int x = -1; x <= 1; x++){

            o = vec2(x, y);
            o += hash22(g + o) - p;

            d.z = dot(o, o);
            // More distance metrics.
            //o = abs(o);
            //d.z = max(o.x*.8666 + o.y*.5, o.y);//
            //d.z = max(o.x, o.y);
            //d.z = (o.x*.7 + o.y*.7);

            d.y = max(d.x, min(d.y, d.z));
            d.x = min(d.x, d.z);

        }
    }

    return max(d.y/1.2 - d.x*1., 0.)/1.2;
    //return d.y - d.x; // return 1.-d.x; // etc.

}



void main() {

    vec2 iResolution = vec2(width, height);
    ivec2 id = ivec2(gl_GlobalInvocationID.xy);
 float tm = (iTime / 2.) * 0.0125;



    // Unit directional ray - Coyote's observation.
    vec3 rd = normalize(vec3(2. * vec2(id) - iResolution.xy, iResolution.y));


    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);

    int x = pos.x;
    int y = pos.y;

    // Check if the pixel should be drawn
    if (x >= border_size && x <= (width - 1) - border_size && y >= title_height && y <= (height - 1) - border_size)
    {
        return;
    }

    // Rotate the XY-plane back and forth. Note that sine and cosine are kind of rolled into one.
    vec2 a = sin(vec2(1.570796, 0) + sin(tm / 4.) * .3);

    // Fabrice's observation.
    rd.xy = mat2(a, -a.y, a.x) * rd.xy;

    // Ray origin. Moving in the X-direction to the right.
  //  vec3 ro = vec3(tm, cos(tm / 4.), 0.);
vec3 ro = vec3(0.0,0.0, 0.);
    // Light position, hovering around behind the camera.
    vec3 lp = ro + vec3(cos(tm / 2.) * .5, sin(tm / 2.) * .5, -.5);

    // Standard raymarching segment. Because of the straightforward setup, not many iterations are necessary.
    float d, t = 0.;
    for (int j = 0; j < 32; j++) {
        d = map(ro + rd * t); // distance to the function.
        t += d * .7;          // Total distance from the camera to the surface.

        // The plane "is" the far plane, so no far=plane break is needed.
        if (d < 0.001)
            break;
    }

    // Edge and curve value. Passed into, and set, during the normal calculation.
    float edge, crv;

    // Surface position, surface normal and light direction.
    vec3 sp = ro + rd * t;
    vec3 sn = getNormal(sp, edge, crv);
    vec3 ld = lp - sp;


    // Coloring and texturing the surface.
    //
    // Height map.
    float c = heightMap(sp.xy);

    // Folding, or wrapping, the values above to produce the snake-like pattern that lines up with the randomly
    // flipped hex cells produced by the height map.
    vec3 fold = cos(vec3(1, 2, 4)*c*6.283);

    // Using the height map value, then wrapping it, to produce a finer grain Truchet pattern for the overlay.
    float c2 = heightMap((sp.xy + sp.z*.025)*6.);
    c2 = cos(c2*6.283*3.);
    c2 = (clamp(c2+.5, 0., 1.));


    // Function based bump mapping. I prefer none in this example, but it's there if you want it.
    //if(temp.x>0. || temp.y>0.) sn = dbF(sp, sn, .001);

    // Surface color value.
    vec3 oC = vec3(1);

    if(fold.x>0.) oC = vec3(1, .05, .1)*c2; // Reddish pink with finer grained Truchet overlay.

    if(fold.x<0.05 && (fold.y)<0.) oC = vec3(1, .7, .45)*(c2*.25 + .75); // Lighter lined borders.
    else if(fold.x<0.) oC = vec3(1, .8, .4)*c2; // Gold, with overlay.

    //oC *= n3D(sp*128.)*.35 + .65; // Extra fine grained noisy texturing.


    // Sending some greenish particle pulses through the snake-like patterns. With all the shininess going
    // on, this effect is a little on the subtle side.
    float p1 = 1.0 - smoothstep(0., .1, fold.x*.5+.5); // Restrict to the snake-like path.
    // Other path.
    //float p2 = 1.0 - smoothstep(0., .1, cos(heightMap(sp.xy + 1. + (iTime*0.0125)/4.)*6.283)*.5+.5);
    float p2 = 1.0 - smoothstep(0., .1, Voronoi(sp.xy*4. + vec2(tm, cos(tm/4.))));
    p1 = (p2 + .25)*p1; // Overlap the paths.
    oC += oC.yxz*p1*p1; // Gives a kind of electron effect. Works better with just Voronoi, but it'll do.




    float lDist = max(length(ld), 0.001); // Light distance.
    float atten = 1./(1. + lDist*.125); // Light attenuation.

    ld /= lDist; // Normalizing the light direction vector.


    float diff = max(dot(ld, sn), 0.); // Diffuse.
    float spec = pow(max( dot( reflect(-ld, sn), -rd ), 0.0 ), 16.); // Specular.
    float fre = pow(clamp(dot(sn, rd) + 1., .0, 1.), 3.); // Fresnel, for some mild glow.

    // Shading. Note, there are no actual shadows. The camera is front on, so the following
    // two functions are enough to give a shadowy appearance.
    crv = crv*.9 + .1; // Curvature value, to darken the crevices.
    float ao = calculateAO(sp, sn); // Ambient occlusion, for self shadowing.



    // Combining the terms above to light the texel.
 vec3 col = oC*(diff + .5) + vec3(1., .7, .4)*spec*2. + vec3(.4, .7, 1)*fre;


   col += (oC*.5+.5)*envMap(reflect(rd, sn), sn)*6.; // Fake environment mapping.


    // Edges.
    col *= 1. - edge*.85; // Darker edges.

    // Applying the shades.
    col *= (atten*crv*ao);

col *= 1.3;
 // Rough gamma correction, then write to the output buffer.
    vec3 finalColor = sqrt(clamp(col, 0., 1.));
    ivec2 fragCoord = ivec2(id);  // Convert to ivec2
    imageStore(imageOut, fragCoord, vec4(finalColor, 1.0));  // Use vec4 instead of uvec4

}
)";

// ported from https://www.shadertoy.com/view/mtyGWy
static const char *render_source_neon_pattern =
    R"(

 #version 310 es
precision highp float;
precision highp image2D;

layout(binding = 0, rgba32f) uniform readonly image2D in_tex;
layout(binding = 0, rgba32f) uniform writeonly image2D out_tex;
layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

//uniform float current_time;  // Assuming this is the global time variable
layout(location = 1) uniform int title_height;
layout(location = 2) uniform int border_size;
layout(location = 5) uniform int width;
layout(location = 6) uniform int height;
layout(location = 7) uniform int radius;
layout(location = 9) uniform float current_time;

vec3 palette(float t) {
    vec3 a = vec3(0.5, 0.5, 0.5);
    vec3 b = vec3(0.5, 0.5, 0.5);
    vec3 c = vec3(1.0, 1.0, 1.0);
    vec3 d = vec3(0.263, 0.416, 0.557);
    return a + b * cos(6.28318 * (c * t + d));
}

void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);

       // Extract x and y coordinates
    int x = pos.x;
    int y = pos.y;

    // Check if the pixel should be drawn
    if (x >= border_size && x <= (width - 1) - border_size && y >= title_height && y <= (height - 1) - border_size)
    {
        return;
    }
    vec2 uv = vec2(pos) / vec2(width, height);
    vec2 uv0 = uv;
    vec3 finalColor = vec3(0.0);

    for (float i = 0.0; i < 4.0; i++) {
        uv = fract(uv * 1.5) - 0.5;

        float d = length(uv) * exp(-length(uv0));

        vec3 col = palette(length(uv0) + i * 0.4 + current_time * 0.000001);

        d = sin(d * 8.0 + current_time * 0.02) / 8.0;
        d = abs(d);

        d = pow(0.01 / d, 1.2);

        finalColor += col * d;
    }

    // Output the final color to the output texture
    imageStore(out_tex, pos, vec4(finalColor, 1.0));
}
)";

// original (by phodius)
static const char *render_source_neon_rings =
    R"(
#version 310 es
precision highp float;
precision highp image2D;

layout(binding = 0, rgba32f) uniform readonly image2D in_tex;
layout(binding = 0, rgba32f) uniform writeonly image2D out_tex;
layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(location = 1) uniform int title_height;
layout(location = 2) uniform int border_size;
layout(location = 5) uniform int width;
layout(location = 6) uniform int height;
layout(location = 7) uniform int radius;
layout(location = 9) uniform float current_time; // Animated time

#define PI 3.14159265358979323846
#define TWO_PI 6.28318530717958647692

void main() {

    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    int x = pos.x;
    int y = pos.y;

    // Check if the pixel should be drawn
    if (x >= border_size && x <= (width - 1) - border_size && y >= title_height && y <= (height - 1) - border_size)
    {
        return;
    }
    // Number of circles
    int numCircles = 5;

    // Initialize final color
    vec3 finalColor = vec3(0.0);

    for (int i = 0; i < numCircles; ++i) {
        // Calculate UV coordinates for each circle
        vec2 uv = vec2(pos) / vec2(width / 3, height / 3);

        // Calculate polar coordinates
        float a = atan(uv.y, uv.x);
        float r = length(uv) * (0.2 + 0.1 * float(i));

        // Map polar coordinates to UV
        uv = vec2(a / TWO_PI, r);

        // Apply horizontal movement based on time for each circle
        float xOffset = sin(current_time * 0.02 + float(i)) * 0.2;
        uv.x += xOffset;

        // Time-dependent color
        float xCol = (uv.x - (current_time / 300000.0)) * 3.0;
        xCol = mod(xCol, 3.0);
        vec3 horColour = vec3(0.25, 0.25, 0.25);

        if (xCol < 1.0) {
            horColour.r += 1.0 - xCol;
            horColour.g += xCol;
        } else if (xCol < 2.0) {
            xCol -= 1.0;
            horColour.g += 1.0 - xCol;
            horColour.b += xCol;
        } else {
            xCol -= 2.0;
            horColour.b += 1.0 - xCol;
            horColour.r += xCol;
        }

        // Draw color beam
        uv = (2.0 * uv) - 1.0;
        float beamWidth = (0.7 + 0.5 * cos(uv.x * 1.0 * TWO_PI * 0.15 * clamp(floor(5.0 + 1.0 * cos(current_time)), 0.0, 1.0))) * abs(1.0 / (30.0 * uv.y));
        vec3 horBeam = vec3(beamWidth);

        // Add the color for the current circle to the final color
        finalColor += ((horBeam) * horColour);
    }

    // Output the final color to the output texture
    imageStore(out_tex, pos, vec4(finalColor, 1.0));
}


)";

// ported from https://www.shadertoy.com/view/WdjGRc
static const char *render_source_deco =
    R"(
#version 320 es
precision highp float;
precision highp image2D;

layout(binding = 0, rgba32f) uniform readonly image2D in_tex;
layout(binding = 0, rgba32f) uniform writeonly image2D out_tex;
layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;


layout(location = 1) uniform int title_height;
layout(location = 2) uniform int border_size;
layout(location = 5) uniform int width;
layout(location = 6) uniform int height;
layout(location = 7) uniform int radius;
layout(location = 9) uniform float current_time;

vec3 effect(float speed, vec2 uv, float time, float scale) {
    float t = mod(time * 0.005, 6.0);
    float rt = 0.00000000000001 * sin(t * 0.45);

    mat2 m1 = mat2(cos(rt), -sin(rt), -sin(rt), cos(rt));
    vec2 uva = uv * m1 * scale;
    float irt = 0.005 * cos(t * 0.05);
    mat2 m2 = mat2(sin(irt), cos(irt), -cos(irt), sin(irt));

    for (int i = 1; i < 400; i += 1) {
        float it = float(i);
        uva *= m2;
        uva.y += -1.0 + (0.6 / it) * cos(t + it * uva.x + 0.5 * it) * float(mod(it, 0.5) == 0.0);
        uva.x += 1.0 + (0.5 / it) * cos(t + it * uva.y * 0.1 / 5.0 + 0.5 * (it + 15.0));  // Adjust the scaling factor for y-coordinate
    }

    float n = 0.5;
    float r = n + n * sin(4.0 * uva.x + t);
    float gb = n + n * sin(3.0 * uva.y);
    return vec3(r, gb * 0.8 * r, gb * r);
}

void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);

    vec2 uv = vec2(pos) / vec2(width, height);
    uv = 2.0 * uv - 1.0;
    uv *= (10.3 + 0.1 * sin(current_time * 0.01));

    int x = pos.x;
    int y = pos.y;

    // Check if the pixel should be drawn
    if (x >= border_size && x <= (width - 1) - border_size && y >= title_height && y <= (height - 1) - border_size)
    {
        return;
    }


   // uv.y -= current_time * 0.013;
//uv.x -= current_time * 0.026;
    vec3 col = effect(0.001, uv, current_time, 0.5);
    // col += effect(0.5, uv * 3.0, 2.0 * current_time + 10.0, 0.5) * 0.3;
    // col += effect(0.5, sin(current_time * 0.01) * uv * 2.0, 2.0 * current_time + 10.0, 0.5) * 0.1;

    // Output the final color to the output texture
    imageStore(out_tex, pos, vec4(col, 1.0));
}
)";

static const char *rounded_corner_overlay =
    R"(
#version 320 es

layout(binding = 0, rgba32f) readonly uniform highp image2D in_tex;  // Use binding point 0
layout(binding = 0, rgba32f) writeonly uniform highp image2D out_tex;  // Use binding point 0

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(location = 1) uniform int title_height;
layout(location = 2) uniform int border_size;
layout(location = 5) uniform int width;
layout(location = 6) uniform int height;
layout(location = 7) uniform int corner_radius;
layout(location = 8) uniform int shadow_radius;
layout(location = 9) uniform vec4 shadow_color;

void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);

    // Check if the pixel should be drawn
    if (pos.x >= border_size && pos.x <= (width - 1) - border_size && pos.y >= title_height && pos.y <= (height - 1) - border_size)
    {
        return;
    }
    float d;
    vec4 c = shadow_color;
    vec4 m = vec4(0.0);
    vec4 s;
    float diffuse = 1.0 / float(shadow_radius == 0 ? 1 : shadow_radius);
    // left
    if (pos.x < shadow_radius * 2 && pos.y >= shadow_radius * 2 && pos.y <= height - shadow_radius * 2)
    {
        d = distance(vec2(float(shadow_radius * 2), float(pos.y)), vec2(pos));
        imageStore(out_tex, pos, mix(c, m, 1.0 - exp(-pow(d * diffuse, 2.0))));
    }
    // top left corner
    if (pos.x < shadow_radius * 2 + corner_radius && pos.y < shadow_radius * 2 + corner_radius)
    {
        d = distance(vec2(float(shadow_radius * 2 + corner_radius)), vec2(pos)) - float(corner_radius);
        s = mix(c, m, 1.0 - exp(-pow(d * diffuse, 2.0)));
        c = imageLoad(in_tex, pos);
        d = distance(vec2(float(shadow_radius * 2 + corner_radius)), vec2(pos));
        imageStore(out_tex, pos, mix(c, s, clamp(d - float(corner_radius), 0.0, 1.0)));
    }
    // bottom left corner
    if (pos.x < (shadow_radius * 2 + corner_radius) && pos.y > height - (shadow_radius * 2 + corner_radius))
    {
        d = distance(vec2(float((shadow_radius * 2 + corner_radius)), float((height - 1) - (shadow_radius * 2 + corner_radius))), vec2(pos)) - float(corner_radius);
        s = mix(c, m, 1.0 - exp(-pow(d * diffuse, 2.0)));
        c = imageLoad(in_tex, pos);
        d = distance(vec2(float((shadow_radius * 2 + corner_radius)), float((height - 1) - (shadow_radius * 2 + corner_radius))), vec2(pos));
        imageStore(out_tex, pos, mix(c, s, clamp(d - float(corner_radius), 0.0, 1.0)));
    }
    // top
    if (pos.x >= shadow_radius * 2 + corner_radius && pos.x <= width - shadow_radius * 2 + corner_radius && pos.y < shadow_radius * 2)
    {
        d = distance(vec2(float(pos.x), float(shadow_radius * 2)), vec2(pos));
        imageStore(out_tex, pos, mix(c, m, 1.0 - exp(-pow(d * diffuse, 2.0))));
    }
    // right
    if (pos.x > (width - 1) - shadow_radius * 2 && pos.y >= shadow_radius * 2 && pos.y <= (height - 1) - shadow_radius * 2)
    {
        d = distance(vec2(float((width - 1) - shadow_radius * 2), float(pos.y)), vec2(pos));
        imageStore(out_tex, pos, mix(c, m, 1.0 - exp(-pow(d * diffuse, 2.0))));
    }
    // top right corner
    if (pos.x > width - (shadow_radius * 2 + corner_radius) && pos.y < (shadow_radius * 2 + corner_radius))
    {
        d = distance(vec2(float((width - 1) - (shadow_radius * 2 + corner_radius)), float((shadow_radius * 2 + corner_radius))), vec2(pos)) - float(corner_radius);
        s = mix(c, m, 1.0 - exp(-pow(d * diffuse, 2.0)));
        c = imageLoad(in_tex, pos);
        d = distance(vec2(float((width - 1) - (shadow_radius * 2 + corner_radius)), float((shadow_radius * 2 + corner_radius))), vec2(pos));
        imageStore(out_tex, pos, mix(c, s, clamp(d - float(corner_radius), 0.0, 1.0)));
    }
    // bottom right corner
    if (pos.x > (width - 1) - (shadow_radius * 2 + corner_radius) && pos.y > (height - 1) - (shadow_radius * 2 + corner_radius))
    {
        d = distance(vec2(float((width - 1) - (shadow_radius * 2 + corner_radius)), float((height - 1) - (shadow_radius * 2 + corner_radius))), vec2(pos)) - float(corner_radius);
        s = mix(c, m, 1.0 - exp(-pow(d * diffuse, 2.0)));
        c = imageLoad(in_tex, pos);
        d = distance(vec2(float((width - 1) - (shadow_radius * 2 + corner_radius)), float((height - 1) - (shadow_radius * 2 + corner_radius))), vec2(pos));
        imageStore(out_tex, pos, mix(c, s, clamp(d - float(corner_radius), 0.0, 1.0)));
    }
    // bottom
    if (pos.x >= (shadow_radius * 2 + corner_radius) && pos.x <= (width - 1) - (shadow_radius * 2 + corner_radius) && pos.y > (height - 1) - shadow_radius * 2)
    {
        d = distance(vec2(float(pos.x), float((height - 1) - shadow_radius * 2)), vec2(pos));
        imageStore(out_tex, pos, mix(c, m, 1.0 - exp(-pow(d * diffuse, 2.0))));
    }
}
)";

static const char *beveled_glass_overlay =
    R"(
#version 320 es

layout(binding = 0, rgba32f) readonly uniform highp image2D neural_network_texture;  // Use binding point 0
layout(binding = 0, rgba32f) writeonly uniform highp image2D image;  // Use binding point 0

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

#define MARKER_RADIUS 12.5
#define THICCNESS 2.0

layout(location = 1) uniform int title_height;
layout(location = 2) uniform int border_size;

layout(location = 5) uniform int width;
layout(location = 6) uniform int height;
layout(location = 7) uniform int radius;

bool isInsideTriangle(vec2 p, vec2 a, vec2 b, vec2 c) {
    vec2 v0 = b - a;
    vec2 v1 = c - a;
    vec2 v2 = p - a;

    float dot00 = dot(v0, v0);
    float dot01 = dot(v0, v1);
    float dot02 = dot(v0, v2);
    float dot11 = dot(v1, v1);
    float dot12 = dot(v1, v2);

    float invDenom = 1.0 / (dot00 * dot11 - dot01 * dot01);
    float u = (dot11 * dot02 - dot01 * dot12) * invDenom;
    float v = (dot00 * dot12 - dot01 * dot02) * invDenom;

    return (u >= 0.0) && (v >= 0.0) && (u + v <= 1.0);
}


vec4 toBezier(float delta, int i, vec4 P0, vec4 P1, vec4 P2, vec4 P3)
{
    float t = delta * float(i);
    float t2 = t * t;
    float one_minus_t = 1.0 - t;
    float one_minus_t2 = one_minus_t * one_minus_t;
    return (P0 * one_minus_t2 * one_minus_t + P1 * 3.0 * t * one_minus_t2 + P2 * 3.0 * t2 * one_minus_t + P3 * t2 * t);
}

float sin01(float x) {
    return (sin(x) + 1.0) / 2.0;
}



vec4 drawCurveBenzier(vec2 p1, vec2 p2, int setCount) {
    vec4 col = vec4(0.0);
 ivec2 storePos = ivec2(gl_GlobalInvocationID.xy);
  float curveRadius = float(radius);
// Draw the lines connecting the points

    vec2 fragCoord = vec2(storePos);
    for (int i = 0; i <= setCount; ++i) {
        vec4 bezierPoint = toBezier(1.0 / float(setCount), i, vec4(p1, 0.0, 1.0), vec4(p1 + (p2 - p1) * 0.5, 0.0, 1.0), vec4(p1 + (p2 - p1) * 0.5, 0.0, 1.0), vec4(p2, 0.0, 1.0));
        vec2 curvePoint = bezierPoint.xy / bezierPoint.w;
        float distance = length(fragCoord - curvePoint);
        float curveIntensity = smoothstep(0.0, THICCNESS, curveRadius - distance);
        col.rgb += vec3(0.1, 0.1, 0.1) * curveIntensity;
        col.a = max(col.a, curveIntensity);
    }

    return col;
}




vec4 drawCurve(vec2 p1, vec2 p2, int setCount) {
    vec4 col = vec4(0.0);
    ivec2 storePos = ivec2(gl_GlobalInvocationID.xy);
    float curveRadius = float(radius + 1);
    vec2 fragCoord = vec2(storePos);

    float t = clamp(dot(fragCoord - p1, p2 - p1) / dot(p2 - p1, p2 - p1), 0.0, 1.0);
    vec2 curvePoint = mix(p1, p2, t);

    float distance = length(fragCoord - curvePoint);
    float curveIntensity = smoothstep(0.0, curveRadius, curveRadius - distance);

    // Calculate gradient along the line with grey to white to grey
    vec3 gradientColor;

    if (t < 0.5) {
        gradientColor = mix(vec3(0.7), vec3(1.0), t * 2.0);
    } else {
        gradientColor = mix(vec3(1.0), vec3(0.7), (t - 0.5) * 2.0);
    }

    // Use the curveIntensity to interpolate between gradient colors
    col.rgb = gradientColor * curveIntensity;
    col.a = max(col.a, curveIntensity);

    return col;
}

void main() {
    ivec2 storePos = ivec2(gl_GlobalInvocationID.xy);

    int x = storePos.x;
    int y = storePos.y;
    // Check if the pixel should be drawn
    if (x >= border_size && x <= (width - 1) - border_size && y >= title_height && y <= (height - 1) - border_size)
    {
        return;
    }
    vec2 fragCoord = vec2(storePos);

 vec4 col = vec4(0.0);
    float space = 15.0;

    vec2 p1 = vec2(0.0 + THICCNESS, float(height) - THICCNESS - space);
    vec2 p2 = vec2(0.0 + THICCNESS, 0.0 + THICCNESS + space);

    vec2 x1 = vec2(float(width) - THICCNESS, float(height) - THICCNESS - space);
    vec2 x2 = vec2(float(width) - THICCNESS, 0.0 + THICCNESS + space);

    vec2 y1 = vec2(0.0 + THICCNESS + space, float(height) - THICCNESS);
    vec2 y2 = vec2(float(width) - THICCNESS - space, float(height) - THICCNESS);

    vec2 z1 = vec2(0.0 + THICCNESS + space, 0.0 + THICCNESS);
    vec2 z2 = vec2(float(width) - THICCNESS - space, 0.0 + THICCNESS);

 float curveRadius = float(radius);
// Draw the lines connecting the points

int setwidth = int(width);
int setheight = int(height);


if (setwidth<=400 && setheight<=300 ){
    col += drawCurveBenzier(p1, p2, setheight);
    col += drawCurveBenzier(x1, x2, setheight);
    col += drawCurveBenzier(y1, y2, setwidth);
    col += drawCurveBenzier(z1, z2, setwidth);
}
else {
    col += drawCurve(p1, p2, setheight);
    col += drawCurve(x1, x2, setheight);
    col += drawCurve(y1, y2, setwidth);
    col += drawCurve(z1, z2, setwidth);
	}


float INNERspace = 0.0;


//left
    vec2 INNERp1 = vec2(0.0 - THICCNESS+ float(border_size), float(height)  - THICCNESS - INNERspace-float(border_size) );
    vec2 INNERp2 = vec2(0.0 - THICCNESS+ float(border_size),0.0 - THICCNESS + INNERspace+float(border_size) +float(title_height-border_size));
//right
    vec2 INNERx1 = vec2(float(width) + THICCNESS-float(border_size), float(height)- THICCNESS - INNERspace-float(border_size));
    vec2 INNERx2 = vec2(float(width) + THICCNESS-float(border_size),0.0 - THICCNESS + INNERspace+float(border_size) +float(title_height-border_size));
//bottom
    vec2 INNERy1 = vec2(0.0 + THICCNESS + INNERspace+float(border_size), float(height)   + THICCNESS -float(border_size));
    vec2 INNERy2 = vec2(float(width) - THICCNESS - INNERspace-float(border_size), float(height) + THICCNESS-float(border_size));
//top
   vec2 INNERz1 = vec2(0.0 +INNERspace+float(border_size), 0.0 - THICCNESS + INNERspace+float(border_size) +float(title_height-border_size));
    vec2 INNERz2 = vec2(float(width) - THICCNESS - INNERspace-float(border_size) ,0.0 - THICCNESS + INNERspace+float(border_size) +float(title_height-border_size));



if (setwidth<=400 && setheight<=300 ){
    col += drawCurveBenzier(INNERp1, INNERp2, setheight);
    col += drawCurveBenzier(INNERx1, INNERx2, setheight);
    col += drawCurveBenzier(INNERy1, INNERy2, setwidth);
    col += drawCurveBenzier(INNERz1, INNERz2, setwidth);
}
else{
    col += drawCurve(INNERp1, INNERp2, setheight);
    col += drawCurve(INNERx1, INNERx2, setheight);
    col += drawCurve(INNERy1, INNERy2, setwidth);
    col += drawCurve(INNERz1, INNERz2, setwidth);
    }


    vec2 p3 = fragCoord;
    vec2 p12 = p2 - p1;
    vec2 p13 = p3 - p1;

    vec2 x3 = fragCoord;
    vec2 x12 = x2 - x1;
    vec2 x13 = x3 - x1;

    vec2 y3 = fragCoord;
    vec2 y12 = y2 - y1;
    vec2 y13 = y3 - y1;

    vec2 z3 = fragCoord;
    vec2 z12 = z2 - z1;
    vec2 z13 = z3 - z1;

    float d = dot(p12, p13) / length(p12); // = length(p13) * cos(angle)
    float dx = dot(x12, x13) / length(x12);
    float dy = dot(y12, y13) / length(y12);
    float dz = dot(z12, z13) / length(z12);

    vec2 p4 = p1 + normalize(p12) * d;
    vec2 x4 = x1 + normalize(x12) * dx;
    vec2 y4 = y1 + normalize(y12) * dy;
    vec2 z4 = z1 + normalize(z12) * dz;

       if (((length(p4 - p3) < THICCNESS && length(p4 - p1) <= length(p12) && length(p4 - p2) <= length(p12)) ||
         (length(x4 - x3) < THICCNESS && length(x4 - x1) <= length(x12) && length(x4 - x2) <= length(x12))) ||
         (length(y4 - y3) < THICCNESS && length(y4 - y1) <= length(y12) && length(y4 - y2) <= length(y12)) ||
         (length(z4 - z3) < THICCNESS && length(z4 - z1) <= length(z12) && length(z4 - z2) <= length(z12))) {
        col = vec4(0.1, 0.1, 0.1, 1.0);
    }


    vec2 INNERp3 = fragCoord;
    vec2 INNERp12 = INNERp2 - INNERp1;
    vec2 INNERp13 = INNERp3 - INNERp1;

    vec2 INNERx3 = fragCoord;
    vec2 INNERx12 = INNERx2 - INNERx1;
    vec2 INNERx13 = INNERx3 - INNERx1;

    vec2 INNERy3 = fragCoord;
    vec2 INNERy12 = INNERy2 - INNERy1;
    vec2 INNERy13 = INNERy3 - INNERy1;

    vec2 INNERz3 = fragCoord;
    vec2 INNERz12 = INNERz2 - INNERz1;
    vec2 INNERz13 = INNERz3 - INNERz1;

    float INNERd = dot(INNERp12, INNERp13) / length(INNERp12); // = length(p13) * cos(angle)
    float INNERdx = dot(INNERx12, INNERx13) / length(INNERx12);
    float INNERdy = dot(INNERy12, INNERy13) / length(INNERy12);
    float INNERdz = dot(INNERz12, INNERz13) / length(INNERz12);

    vec2 INNERp4 = INNERp1 + normalize(INNERp12) * INNERd;
    vec2 INNERx4 = INNERx1 + normalize(INNERx12) * INNERdx;
    vec2 INNERy4 = INNERy1 + normalize(INNERy12) * INNERdy;
    vec2 INNERz4 = INNERz1 + normalize(INNERz12) * INNERdz;

       if (((length(INNERp4 - INNERp3) < THICCNESS && length(INNERp4 - INNERp1) <= length(INNERp12) && length(INNERp4 - INNERp2) <= length(INNERp12)) ||
         (length(INNERx4 - INNERx3) < THICCNESS && length(INNERx4 - INNERx1) <= length(INNERx12) && length(INNERx4 - INNERx2) <= length(INNERx12))) ||
         (length(INNERy4 - INNERy3) < THICCNESS && length(INNERy4 - INNERy1) <= length(INNERy12) && length(INNERy4 - INNERy2) <= length(INNERy12)) ||
         (length(INNERz4 - INNERz3) < THICCNESS && length(INNERz4 - INNERz1) <= length(INNERz12) && length(INNERz4 - INNERz2) <= length(INNERz12))) {
        col = vec4(0.1, 0.1, 0.1, 1.0);
    }





    // Draw the lines connecting the points
       // Calculate the line segment connecting p1 and y1
    vec2 dir = normalize(y1 - p1);
    vec2 relPoint = fragCoord - p1;
    float alongLine = dot(relPoint, dir);
    float distance = length(relPoint - alongLine * dir);

    // Draw the line connecting p1 and y1
    if (distance < THICCNESS && alongLine > 0.0 && alongLine < length(y1 - p1)) {
        col = vec4(0.1, 0.1, 0.1, 1.0);
    }



    dir = normalize(y2 - x1);
    relPoint = fragCoord - x1;
     alongLine = dot(relPoint, dir);
     distance = length(relPoint - alongLine * dir);

    if (distance < THICCNESS && alongLine > 0.0 && alongLine < length(x1 - y2)) {
        col = vec4(0.1, 0.1, 0.1, 1.0);
    }


 dir = normalize(p2 - z1);
    relPoint = fragCoord - z1;
     alongLine = dot(relPoint, dir);
     distance = length(relPoint - alongLine * dir);

    if (distance < THICCNESS && alongLine > 0.0 && alongLine < length(z1 - p2)) {
        col = vec4(0.1, 0.1, 0.1, 1.0);
    }



 dir = normalize(x2 - z2);
    relPoint = fragCoord - z2;
     alongLine = dot(relPoint, dir);
     distance = length(relPoint - alongLine * dir);

    if (distance < THICCNESS && alongLine > 0.0 && alongLine < length(x2 - z2)) {
        col = vec4(0.1, 0.1, 0.1, 1.0);
    }
    // Draw other lines...

    vec2 t1 = vec2(0.0, float(height));
    vec2 t2 = vec2(float(width), float(height));
    vec2 t3 = vec2(-0.5, -0.5);
    vec2 t4 = vec2(float(width), -0.5);


   if (isInsideTriangle(fragCoord, vec2(p1.x-10.1,p1.y), t1, vec2(y1.x,y1.y+10.1))  ||
        isInsideTriangle(fragCoord, vec2(x1.x+10.1,x1.y), t2, vec2(y2.x,y2.y+10.1))  ||
        isInsideTriangle(fragCoord, vec2(z1.x,z1.y-2.4), t3, vec2(p2.x-3.1,p2.y-2.1)) ||
        isInsideTriangle(fragCoord, vec2(z2.x,z2.y-5.5), t4, vec2(x2.x+3.1,x2.y)) ) {
        col = vec4(0.0, 0.0, 0.0, 0.0);  // Set the color to fully transparent
    } else {
        // Apply your original logic for other regions

        // Sample neural network texture
        vec4 neuralColor = imageLoad(neural_network_texture, ivec2(fragCoord));
        col += neuralColor;
    }
//z2 - x2

  //    vec4 neuralColor = imageLoad(neural_network_texture, ivec2(fragCoord));
   //     col += neuralColor;
    // Store the final color
    imageStore(image, storePos, col);
}

)";

static
const char* gtk_overlay =
    R"(
#version 320 es

precision highp float;
precision highp image2D;

// Adwaita — 2, "macOS-like" themes — 3–4
#define MAX_SHADOWS 4

layout(binding = 0, rgba32f) uniform writeonly image2D out_tex;
layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

// Window and element sizes
// ---- geometry: LOGICAL px (float, scale == 1) ----
layout(location = 1)  uniform float u_titlebar_height;   // 28 — from the theme (.csd .. -> min-height)
layout(location = 2)  uniform vec4  u_titlebar_border;   // Titlebar (T,R,B,L) titlebar borders: (0,0,1,0)
layout(location = 3)  uniform vec4  u_titlebar_shadow;   // inset 0 1px 0 0
layout(location = 4)  uniform vec4  u_frame_border;      // Window frame (T, R, B, L) - separate element
// buffer
layout(location = 5)  uniform int   u_width;             // width(int)
layout(location = 6)  uniform int   u_height;            // height(int)
// Frame radius around the window
layout(location = 7)  uniform vec4  u_frame_radius;      // Shadow (TL,TR,BR,BL) = (3,3,0,0)
// ---- palette (ARK Dark) ----
layout(location = 8)  uniform vec4  u_titlebar_color;      // #2f343f/#323843 — titlebar background color
layout(location = 9)  uniform vec4  u_titlebar_border_color; // #262a33/#262a33 — titlebar border color
layout(location = 10) uniform vec4  u_titlebar_shadow_color; // rgba(54,59,72,0.97) — shadow (inset stroke) color on the titlebar
layout(location = 11) uniform vec4  u_frame_border_color;  // rgba(0,0,0,0.0) transparent — window border color
// ---- Decoration geometry ----
// — static, client position in the buffer —
// (L, R, B, T) — like wf::decoration_margins_t
layout(location = 12) uniform vec4  u_margins;           // LOGICAL px (23,23,23+8,23-8)

// --- titlebar gradient: #262626 (bottom) -> #2b2b2b (top) ---
// set both to vec4(0) - if only u_titlebar_color is needed
layout(location = 13) uniform vec4  u_titlebar_grad_bottom;    // #262626
layout(location = 14) uniform vec4  u_titlebar_grad_top;       // #2b2b2b

// number of shadows
layout(location = 15) uniform int   u_shadow_count;
// ---- shadow: offset-x offset-y σ spread ----
// σ = ½·r·¾√2π
// box-shadow: 0 0 0 1px
// box-shadow: 0 8px 7.52(8px) 0 / box-shadow: 0 5px 4.7(5px) 0
// (offset_x, offset_y, σ, spread), px
layout(location = 16) uniform vec4 u_shadow_geom[MAX_SHADOWS];
// rgba(128,128,128,1.0)      | rgba(0,0,0,0.35)
// rgba(32,35,43,0.97)/rgba(32,35,43,0.87)
// ---- shadow: color (rgba) ----
layout(location = 20) uniform vec4 u_shadow_color[MAX_SHADOWS];

layout(location = 25) uniform float u_scale;

// ---- Helper functions ----

// Analytic signed distance function (SDF) for a rounded rectangle
// Coordinate system: x right, y DOWN, p — relative to the window center.
float sdRoundedRect(vec2 p, vec2 b, vec4 r) {
    float cr = (p.x < 0.0)
        ? ((p.y < 0.0) ? r.x : r.w)   // left half:  TL / BL
        : ((p.y < 0.0) ? r.y : r.z);  // right half: TR / BR
    cr = min(cr, min(b.x, b.y));      // radius no larger than half a side
    vec2 q = abs(p) - b + vec2(cr);
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - cr;
}

// SDF with ELLIPTICAL corners (rx, ry per corner TL,TR,BR,BL).
// rx==ry — bit-for-bit the old circle; zero semi-axis — miter.
float sdRoundedRectE(vec2 p, vec2 center, vec4 rx, vec4 ry) {
    vec2 r = vec2(
        (p.x < 0.0) ? ((p.y < 0.0) ? rx.x : rx.w) : ((p.y < 0.0) ? rx.y : rx.z),
        (p.x < 0.0) ? ((p.y < 0.0) ? ry.x : ry.w) : ((p.y < 0.0) ? ry.y : ry.z));

    r = min(r, center);
    // Zero semi-axis: the arc degenerates into a segment lying ON the edge line,
    // => true miter — a sharp corner of the FULL box. Zero BOTH semi-axes,
    // otherwise (rx>0, ry==0) shifts the corner inward by rx and cuts the edge.
    vec2 q = abs(p) - center;
    float corner;
    if (r.x > 0.0 && r.y > 0.0) {
      // pseudo-SDF: the 0-level is an exact ellipse
      q += r;
      vec2 w = max(q, vec2(0.0));
      corner = (length(w / r) - 1.0) * min(r.x, r.y);
    } else {
        // degenerate corner — miter, as before
        vec2 w = max(q, vec2(0.0));
        corner = length(w);
    }

    return min(max(q.x, q.y), 0.0) + corner;
}

// pick_corner_radius/sdRoundedRectCr: experimental variant against
// corner radius selection shifting due to offset. In practice it gives no
// visual improvement (verified), currently unused. Kept as
// groundwork in case this variant needs to be restored later.

// radius selection — by the UNSHIFTED point (true window geometry),
// apply the offset only to the distance itself, not to which corner we pick
float pick_corner_radius(vec2 rel, vec4 r) {
    return (rel.x < 0.0)
         ? ((rel.y < 0.0) ? r.x : r.w)
         : ((rel.y < 0.0) ? r.y : r.z);
}

float sdRoundedRectCr(vec2 p, vec2 b, float cr) {
    cr = min(cr, min(b.x, b.y));
    vec2 q = abs(p) - b + vec2(cr);
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - cr;
}

// erf (Abramowitz–Stegun 7.1.26) — for the blurred edge profile 0.5*erfc(d/(σ√2))
float erf(float x) {
    float s = sign(x), ax = abs(x);
    float t = 1.0 / (1.0 + 0.3275911 * ax);
    float y = 1.0 - (((((1.061405429 * t - 1.453152027) * t
             + 1.421413741) * t - 0.284496736) * t + 0.254829592) * t)
             * exp(-ax * ax);
    return s * y;
}

// Portable Porter-Duff layer compositing function (Over)
vec4 over(vec4 src, vec4 dst) {
    float a = src.a + dst.a * (1.0 - src.a);
    vec3 rgb = (src.rgb * src.a + dst.rgb * dst.a * (1.0 - src.a)) / max(a, 1e-6);
    return vec4(rgb, a);
}

// ordered 4x4 dithering (Bayer): static between repaints,
// does not distort the average color
float bayer4(vec2 p) {
    int x = int(mod(p.x, 4.0)), y = int(mod(p.y, 4.0));
    const float B[16] = float[16](
         0.,  8.,  2., 10.,  12.,  4., 14.,  6.,
         3., 11.,  1.,  9.,  15.,  7., 13.,  5.);
    return (B[y * 4 + x] + 0.5) / 16.0 - 0.5;   // [-0.469 .. +0.469] LSB
}

// simple ordered dither (Bayer-like, without a pattern texture)
// for sub-pixel blur
float dither(ivec2 pos) {
    // hash → [0,1), deterministic per pixel coordinate
    float h = fract(sin(dot(vec2(pos), vec2(12.9898, 78.233))) * 43758.5453);
    return (h - 0.5) / 255.0;   // amplitude ±0.5 LSB per 8-bit channel
}

// select border width by side
float pick_border_width(vec2 rel, vec2 half_, vec4 w, float corner_r) {
    float dx = abs(rel.x) - half_.x;
    float dy = abs(rel.y) - half_.y;

    float w_horiz = rel.y < 0.0 ? w.x : w.z;   // top / bottom
    float w_vert  = rel.x < 0.0 ? w.w : w.y;   // left / right

    if (corner_r <= 0.0) {
        // sharp corner — true miter, the jump is geometrically correct
        return (dx > dy) ? w_vert : w_horiz;
    }

    // rounded corner — blend smoothly in a ~radius band around the diagonal
    float t = smoothstep(-corner_r, corner_r, dx - dy);
    return mix(w_horiz, w_vert, t);
}

void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    if (pos.x >= u_width || pos.y >= u_height) return;

    vec4  border = u_scale * u_frame_border; // Frame border size
    bool  has_border = any(greaterThan(border, vec4(0.0)));
    vec4  radius = u_scale * u_frame_radius; // Frame border radius

    int shadow_count = u_shadow_count;
    if (shadow_count > MAX_SHADOWS) shadow_count = MAX_SHADOWS;

    // — static, client position in the buffer —
    // (L, R, B, T) — like wf::decoration_margins_t
    vec4 margins;
    if (u_shadow_count > 0) {
        margins = u_scale * u_margins;
    } else //  (T, R, B, L)  ==> (L, R, B, T)
        margins = u_scale * vec4(u_frame_border.w, vec2(u_frame_border.yz), u_frame_border.x);
    // ---- ROUNDING CONTRACT (layout) ----
    // Physical rectangle = floor(logical * scale + 0.5): float32,
    // OPERATION ORDER as here. The host places the client surface
    // using the SAME formula and the same values.
    // Forbidden: round() — ties are implementation-defined; lround/nearbyint —
    // half-even; floorf on the host must be with +0.5f, not roundf.
    // margins = floor(margins + vec4(0.5));
    // (L, R, B, T) — like wf::decoration_margins_t
    vec2 lo = vec2(margins.x, margins.w);                         // (L, T)
    vec2 hi = vec2(float(u_width), float(u_height)) - margins.yz; // (R, B)
    float decoration_top = lo.y; // top of the titlebar
    float px_u = 0.0;                                 // physical px of slack

    // --- SNAP the titlebar strip: match the rasterized client boundary ---
    vec2 lo_s  = ceil(lo - vec2(0.5));        // (L, T) — first texel inside
    vec2 hi_s  = ceil(hi - vec2(0.5));        // (R, B) — first texel outside
    vec2  ctr   = (lo_s + hi_s) * 0.5;
    vec2  half_ = (hi_s - lo_s) * 0.5;
    // --- snap the border: integer thicknesses in texels ---
    vec2  bw = floor(vec2(border.w, border.x) + vec2(0.5));  // (L, T)
    vec2  be = floor(vec2(border.y, border.z) + vec2(0.5));  // (R, B)
    vec2  lo_outer = lo_s - bw;
    vec2  hi_outer = hi_s + be;
    // OUTER SDF: snapped box, full radius
    vec2  ctr_outer  = (lo_outer + hi_outer) * 0.5;
    vec2  half_outer = (hi_outer - lo_outer) * 0.5;
    vec2  rel_outer  = vec2(pos) + 0.5 - ctr_outer;

    // CONTENT SDF: snapped box, radii — from the snapped widths (see below)
    // Mask SDF: straight edges hard, corner arcs soft
    vec4  r_in_x = max(radius - vec4(bw.x, be.x, be.x, bw.x), vec4(0.0)); // TL,TR,BR,BL − (L,R,R,L)
    vec4  r_in_y = max(radius - vec4(bw.y, bw.y, be.y, be.y), vec4(0.0)); // (T,T,B,B)
#if 1
    vec4  r_in = min(r_in_x, r_in_y);
    float d = sdRoundedRect(vec2(pos) + 0.5 - ctr, half_, r_in);
    float d_outer = has_border
        ? sdRoundedRect(vec2(pos) + 0.5 - ctr_outer, half_outer, radius)
        : d;
#else
    // Ellipse approximation
    float d = sdRoundedRectE(vec2(pos) + 0.5 - ctr, half_, r_in_x, r_in_y);
    float d_outer = has_border
        ? sdRoundedRectE(vec2(pos) + 0.5 - ctr_outer, half_outer, radius, radius)
        : d;
#endif
    vec4 titlebar_color = vec4(0.0);
    if (u_titlebar_height > 0.0) {
        float tb_height        = u_scale * u_titlebar_height;
        float tb_shadow_height = u_scale * u_titlebar_shadow.y;
        vec4  tb_border        = u_scale * u_titlebar_border;

        float extent_top = decoration_top + tb_height;
        float fx = float(pos.x) + 0.5;
        float fy = float(pos.y) + 0.5;

        // --- titlebar: ONLY in its own strip, otherwise we'll paint over the client ---
        // --- SNAP the titlebar strip: match the rasterized client boundary ---
        float ext_s = ceil(lo.y + u_scale * u_titlebar_height - 0.5);  // snap the FINAL position

        // --- titlebar: ONLY in its own strip, otherwise we'll paint over the client ---
        if (fx >= lo_s.x && fx < hi_s.x && fy >= lo_s.y && fy < ext_s + px_u) {
            // inner lines — from the snapped edges
            float d_t = fy - lo_s.y;    float d_b = ext_s - fy;
            float d_l = fx - lo_s.x;    float d_r = hi_s.x - fx;

            // --- GRADIENT = base fill of the strip ---
            float rows = ext_s - lo_s.y;  // integer: ceil − ceil
            float t    = clamp((d_b - 0.5) / max(rows - 1.0, 1.0), 0.0, 1.0);
            // t=1 on the top row of the strip, t=0 on the bottom (endpoint-exact)
            vec4 grad = mix(u_titlebar_grad_bottom,   // #262626 — bottom (at the client)
                            u_titlebar_grad_top,      // #2b2b2b — top (at the border)
                            t);
            // background: image OVER background-color
            titlebar_color = over(grad, u_titlebar_color);
            // ±0.5 LSB of the final color, RGB only
            titlebar_color.rgb += bayer4(vec2(pos)) * (1.0 / 255.0);
            // shadow — from the LOCAL gradient color
            // Blend with the result of the previous stage (was u_titlebar_color)!
            vec4 hi_layer = vec4(
                mix(titlebar_color.rgb, u_titlebar_shadow_color.rgb,
                                        u_titlebar_shadow_color.a), titlebar_color.a);
            // Substruct titlebar border top from dt_t
            float cov_h = clamp(tb_shadow_height + 0.5 - (d_t - tb_border.x), 0.0, 1.0);
            titlebar_color = mix(titlebar_color, hi_layer, cov_h);
            // (TL,TR,BR,BL)  (T,R,B,L)
            /* if (radius.x > (border.x + border.w) || radius.y > (border.x + border.y)) { */
                // AA of rounded corners
                titlebar_color.a *= clamp(0.5 -   d, 0.0, 1.0)   // left/right/top + corners
                                 *  clamp(0.5 + d_b, 0.0, 1.0);  // bottom (aa_b)
            /*} else {
                // AA of square corners
                float aa_l = clamp(fx - lo.x           + 0.5, 0.0, 1.0); // d = lo.x − fx -> 0.5 − d = fx − lo.x + 0.5
                float aa_r = clamp(hi.x - fx           + 0.5, 0.0, 1.0); // ← hi.x − fx, NOT fx − hi.x
                float aa_t = clamp(fy - decoration_top + 0.5, 0.0, 1.0); // d = dt − fy
                float aa_b = clamp(extent_top - fy     + 0.5, 0.0, 1.0);
                titlebar_color.a *= aa_l * aa_r * aa_t * aa_b;
            }*/
            // Titlebar border on top of everything
            float cov_b = max(
                max(clamp(tb_border.x + 0.5 - d_t, 0.0, 1.0),
                    clamp(tb_border.y + 0.5 - d_r, 0.0, 1.0)),
                max(clamp(tb_border.z + 0.5 - d_b, 0.0, 1.0),
                    clamp(tb_border.w + 0.5 - d_l, 0.0, 1.0)));

            titlebar_color = mix(titlebar_color, u_titlebar_border_color, cov_b);
        }
    }
    vec4 outside = vec4(0.0);
    if (u_shadow_count > 0) {
        // CSS: the shadow is not drawn inside the border-box — common clip for all layers
        float clip_box = clamp(d_outer + px_u + 0.5, 0.0, 1.0);

        for (int i = 0; i < shadow_count; i++) {
            vec4  g     = u_shadow_geom[i] * u_scale;
            vec2  off   = g.xy;
            float sigma = g.z;
            float s     = g.w; // spread, allow < 0
            // shadow shape: border-box expanded by spread (radii grow by s)
            vec2 b_s = half_outer + s;
            vec4 r_s = max(radius + s, vec4(0.0));
            if (b_s.x <= 0.0 || b_s.y <= 0.0) continue; // degenerate negative spread

            vec4  r_eff = sqrt(r_s * r_s + vec4(sigma * sigma));  // σ=0 → r+s, suitable for a ring
            float ds    = sdRoundedRect(rel_outer - off, b_s, r_eff);

            float cov = (sigma > 0.0)
                ? 0.5 * (1.0 - erf(ds / (sigma * 1.41421356237)))
                : clamp(0.5 - ds, 0.0, 1.0);                 // ring: hard edge with AA
            cov *= clip_box;
            // first in the list — on top
            outside = over(vec4(u_shadow_color[i].rgb, u_shadow_color[i].a * cov), outside);
        }
    }
    // --- border: strip EXACTLY between the content (d=0) and the outer contour (d_outer=0) ---
    if (has_border) {
        float aa_in  = clamp(d + px_u + 0.5, 0.0, 1.0);   // ramps up coming out of the content
        float aa_out = clamp(-d_outer + 0.5, 0.0, 1.0);   // ramps down before the outer contour
        float rc_b   = aa_in * aa_out;

        vec4 border_layer = vec4(u_frame_border_color.rgb, u_frame_border_color.a * rc_b);
        outside = over(border_layer, outside);
    }

    vec4 outc = over(titlebar_color, outside);

    imageStore(out_tex, pos, vec4(outc.rgb * outc.a, outc.a));
}
)";

void setup_shader(GLuint *program, std::string source)
{
    auto compute_shader  = OpenGL::compile_shader(source.c_str(), GL_COMPUTE_SHADER);
    auto compute_program = GL_CALL(glCreateProgram());
    GL_CALL(glAttachShader(compute_program, compute_shader));
    GL_CALL(glLinkProgram(compute_program));

    int s = GL_FALSE;
#define LENGTH 1024 * 128
    char log[LENGTH];
    GL_CALL(glGetProgramiv(compute_program, GL_LINK_STATUS, &s));
    GL_CALL(glGetProgramInfoLog(compute_program, LENGTH, NULL, log));

    if (s == GL_FALSE)
    {
        LOGE("Failed to link shader:\n", source,
            "\nLinker output:\n", log);
    }

    GL_CALL(glDeleteShader(compute_shader));
    *program = compute_program;
}

static void seed_random()
{
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    srandom(ts.tv_nsec);
}

void shader_t::destroy_programs()
{
    if (motion_program != GLuint(-1))
    {
        GL_CALL(glDeleteProgram(motion_program));
        GL_CALL(glDeleteProgram(diffuse1_program));
        GL_CALL(glDeleteProgram(diffuse2_program));
        GL_CALL(glDeleteProgram(project1_program));
        GL_CALL(glDeleteProgram(project2_program));
        GL_CALL(glDeleteProgram(project3_program));
        GL_CALL(glDeleteProgram(project4_program));
        GL_CALL(glDeleteProgram(project5_program));
        GL_CALL(glDeleteProgram(project6_program));
        GL_CALL(glDeleteProgram(advect1_program));
        GL_CALL(glDeleteProgram(advect2_program));
    }

    if (render_program != GLuint(-1))
    {
        GL_CALL(glDeleteProgram(render_program));
    }

    if (render_overlay_program != GLuint(-1))
    {
        GL_CALL(glDeleteProgram(render_overlay_program));
    }

    motion_program = diffuse1_program = diffuse2_program
                   = project1_program = project2_program
                   = project3_program = project4_program
                   = project5_program = project6_program
                   = advect1_program  = advect2_program
                   = render_program   = render_overlay_program
                   = GLuint(-1);
}

const char* effect_type_text(effect_t type);
const char* overlay_engine_text(overlay_t overlay);


void shader_t::create_programs()
{
    destroy_programs();
#ifdef __DEBUG_SHADER__
    LOGW("Effect:  ", effect_type_text(theme_t::effect_type));
    LOGW("Overlay: ", overlay_engine_text(theme_t::overlay_engine));
#endif
    wf::gles::run_in_context([&]
    {
        switch (theme_t::effect_type)
        {
        case effect_t::smoke:
        case effect_t::ink:
            setup_shader(&motion_program, motion_source);
            setup_shader(&diffuse1_program, stitch_smoke_shader(diffuse1_source));
            setup_shader(&diffuse2_program, stitch_smoke_shader(diffuse2_source));
            setup_shader(&project1_program, stitch_smoke_shader(project1_source));
            setup_shader(&project2_program, stitch_smoke_shader(project2_source));
            setup_shader(&project3_program, stitch_smoke_shader(project3_source));
            setup_shader(&project4_program, stitch_smoke_shader(project4_source));
            setup_shader(&project5_program, stitch_smoke_shader(project5_source));
            setup_shader(&project6_program, stitch_smoke_shader(project6_source));
            setup_shader(&advect1_program,  stitch_smoke_shader(advect1_source));
            setup_shader(&advect2_program,  stitch_smoke_shader(advect2_source));
            setup_shader(&render_program, std::string(render_source) + effect_run_for_region_main);
            break;

        case effect_t::clouds:
            setup_shader(&render_program, render_source_clouds);
            break;
        case effect_t::halftone:
            setup_shader(&render_program, render_source_halftone);
            break;
        case effect_t::pattern:
            setup_shader(&render_program, render_source_pattern);
            break;
        case effect_t::lava:
            setup_shader(&render_program, render_source_lava);
            break;
        case effect_t::hex:
            setup_shader(&render_program, render_source_hex);
            break;
        case effect_t::zebra:
            setup_shader(&render_program, render_source_zebra);
            break;
        case effect_t::neural_network:
            setup_shader(&render_program, render_source_neural_network);
            break;
        case effect_t::hexagon_maze:
            setup_shader(&render_program, render_source_hexagon_maze);
            break;
        case effect_t::raymarched_truchet:
            setup_shader(&render_program, render_source_raymarched_truchet);
            break;
        case effect_t::neon_pattern:
            setup_shader(&render_program, render_source_neon_pattern);
            break;
        case effect_t::neon_rings:
            setup_shader(&render_program, render_source_neon_rings);
            break;
        case effect_t::deco:
            setup_shader(&render_program, render_source_deco);
            break;
        case effect_t::none:
            switch (theme_t::overlay_engine)
            {
            case overlay_t::rounded_corners:
                setup_shader(&render_overlay_program, rounded_corner_overlay);
                break;
            case overlay_t::beveled_glass:
                setup_shader(&render_overlay_program, beveled_glass_overlay);
                break;
            case overlay_t::gtk_theme:
#ifdef __DEBUG_SHADER__
                LOGW("setup GTK theme shader");
#endif
                setup_shader(&render_overlay_program, gtk_overlay);
                upload_box_shadow_static(render_overlay_program);
                break;
            case overlay_t::none:
                break;
            }
        }
    });
}

shader_t::shader_t()
        : px_width(-1)
        , px_height(-1)
{
    motion_program = diffuse1_program = diffuse2_program = project1_program =
        project2_program     = project3_program = project4_program = project5_program =
            project6_program = advect1_program = advect2_program = render_program =
                render_overlay_program = GLuint(-1);

    texture = b0u = b0v = b0d = b1u = b1v = b1d = GLuint(-1);

    create_programs();
    seed_random();
}

shader_t::~shader_t()
{
    destroy_programs();
    destroy_textures();
}

void shader_t::create_textures()
{
    GL_CALL(glGenTextures(1, &texture));
    GL_CALL(glGenTextures(1, &b0u));
    GL_CALL(glGenTextures(1, &b0v));
    GL_CALL(glGenTextures(1, &b0d));
    GL_CALL(glGenTextures(1, &b1u));
    GL_CALL(glGenTextures(1, &b1v));
    GL_CALL(glGenTextures(1, &b1d));
}

void shader_t::destroy_textures()
{
    if (texture != GLuint(-1))
    {
        GL_CALL(glDeleteTextures(1, &texture));
        texture = GLuint(-1);
    }

    if (b0u == GLuint(-1))
    {
        return;
    }

    GL_CALL(glDeleteTextures(1, &b0u));
    GL_CALL(glDeleteTextures(1, &b0v));
    GL_CALL(glDeleteTextures(1, &b0d));
    GL_CALL(glDeleteTextures(1, &b1u));
    GL_CALL(glDeleteTextures(1, &b1v));
    GL_CALL(glDeleteTextures(1, &b1d));

    b0u = b0v = b0d = b1u = b1v = b1d = GLuint(-1);
}

int round_up_div(int a, int b)
{
    return (a + b - 1) / b;
}

void shader_t::dispatch_region(const wf::region_t& region)
{
    std::vector<int> values;

    int max_x = 0;
    int max_y = 0;

    for (auto& box : region)
    {
        auto rect = wlr_box_from_pixman_box(box);
        values.push_back(rect.x);
        values.push_back(rect.y);
        values.push_back(rect.width);
        values.push_back(rect.height);

        if (rect.width > rect.height)
        {
            values.push_back(0); // xy will not be flipped in compute shader
            max_x = std::max(max_x, rect.width);
            max_y = std::max(max_y, rect.height);
        } else
        {
            values.push_back(1); // xy will be flipped in compute shader
            max_x = std::max(max_x, rect.height);
            max_y = std::max(max_y, rect.width);
        }
    }

    if (values.size() / 5 > 4)
    {
        LOGE("Error: too many regions");
        return;
    }

    GL_CALL(glUniform1iv(10, values.size(), values.data()));
    GL_CALL(glDispatchCompute(round_up_div(max_x, 16), round_up_div(max_y, 16), values.size() / 5));
    GL_CALL(glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT));
}

void shader_t::render_region(GLuint program, const wf::region_t& region, const wf::dimensions_t& size)
{
    GL_CALL(glUseProgram(program));
    GL_CALL(glUniform1i(5, size.width));
    GL_CALL(glUniform1i(6, size.height));
    dispatch_region(region);
}

void shader_t::recreate_textures()
{
    if ((px_width <= 0) || (px_height <= 0))
    {
        return;
    }

    destroy_textures();

    create_textures();

    GL_CALL(glActiveTexture(GL_TEXTURE0 + 0));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, texture));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));

    if ((theme_t::effect_type == effect_t::none) && (theme_t::overlay_engine == overlay_t::gtk_theme))
    {
        // GLM: Wrap mode.
        // The code sets only NEAREST filters;
        // wrap remains the default GL_REPEAT.
        // With a quad whose coordinates are exactly 0..1, the edge texels may be sampled
        // from the opposite edge (u=1.0 is the boundary).
        GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
        GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    }
    GL_CALL(glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, px_width, px_height));

	bool fluid = (theme_t::effect_type == effect_t::smoke) || (theme_t::effect_type == effect_t::ink);
    if (!fluid)
    {
        return;
    }
    // smoke / ink
    GLuint fb;
    GL_CALL(glGenFramebuffers(1, &fb));
    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, fb));

    wf::color_t clear_color{0, 0, 0, 0};
    GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D, texture, 0));
    OpenGL::clear(clear_color, GL_COLOR_BUFFER_BIT);

    GL_CALL(glActiveTexture(GL_TEXTURE0 + 1));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, b0u));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    GL_CALL(glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32F, px_width, px_height));
    GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D, b0u, 0));
    OpenGL::clear(clear_color, GL_COLOR_BUFFER_BIT);
    GL_CALL(glActiveTexture(GL_TEXTURE0 + 2));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, b0v));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    GL_CALL(glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32F, px_width, px_height));
    GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D, b0v, 0));
    OpenGL::clear(clear_color, GL_COLOR_BUFFER_BIT);
    GL_CALL(glActiveTexture(GL_TEXTURE0 + 3));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, b0d));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    GL_CALL(glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32F, px_width, px_height));
    GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D, b0d, 0));
    OpenGL::clear(clear_color, GL_COLOR_BUFFER_BIT);
    GL_CALL(glActiveTexture(GL_TEXTURE0 + 4));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, b1u));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    GL_CALL(glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32F, px_width, px_height));
    GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D, b1u, 0));
    OpenGL::clear(clear_color, GL_COLOR_BUFFER_BIT);
    GL_CALL(glActiveTexture(GL_TEXTURE0 + 5));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, b1v));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    GL_CALL(glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32F, px_width, px_height));
    GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D, b1v, 0));
    OpenGL::clear(clear_color, GL_COLOR_BUFFER_BIT);
    GL_CALL(glActiveTexture(GL_TEXTURE0 + 6));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, b1d));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    GL_CALL(glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32F, px_width, px_height));
    GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D, b1d, 0));
    OpenGL::clear(clear_color, GL_COLOR_BUFFER_BIT);

    GL_CALL(glDeleteFramebuffers(1, &fb));
}

void shader_t::render(const wf::scene::render_instruction_t& data,
                      const wf::geometry_t& rectangle, float scale)
{
#if 0
    // --- quad: physical 1:1 with the texture ---
    // origin — on a whole physical pixel (damps phase jitter with
    // fractional scale; floor(+0.5) — house convention);
    // size — exactly in texels px_width×px_height (damps stretch drift
    // at the far edge of the window).
    wf::geometry_t q;
    q.x      = px_round<double>(scale * rectangle.x) / scale;
    q.y      = px_round<double>(scale * rectangle.y) / scale;
    q.width  = px_round<double>(scale * rectangle.width)  / scale;
    q.height = px_round<double>(scale * rectangle.height) / scale;
    LOGW(fmt_geom(rectangle, " shader::render() rect"), " scale=", scale, fmt_geom(q, " tex"));
    // draw q; expand the node bbox for damage by 1px (the quad may protrude up to 0.5px)
    OpenGL::render_transformed_texture(wf::gles_texture_t{texture}, q,
        wf::gles::render_target_orthographic_projection(data.target), glm::vec4{1},
        OpenGL::TEXTURE_TRANSFORM_INVERT_Y | OpenGL::RENDER_FLAG_CACHED);
#else
    OpenGL::render_transformed_texture(wf::gles_texture_t{texture}, rectangle,
        wf::gles::render_target_orthographic_projection(data.target), glm::vec4{1},
        OpenGL::TEXTURE_TRANSFORM_INVERT_Y | OpenGL::RENDER_FLAG_CACHED);
#endif
    data.pass->custom_gles_subpass(data.target, [data]
    {
        wf::gles::for_each_scissor_rect(data.target, data.damage, [] ()
        {
            OpenGL::draw_cached();
        });
    });

    OpenGL::clear_cached();
}

#ifdef __DEBUG_SHADER_RENDER__

void shader_t::dump_box_shadow_shader(int Wp, int Hp, float scale)
{
    GLuint fbo; GLint old_fbo;
    GL_CALL(glGetIntegerv(GL_FRAMEBUFFER_BINDING, &old_fbo));
    GL_CALL(glGenFramebuffers(1, &fbo));
    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, fbo));
    GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0));
    std::vector<float> px((size_t)Wp * Hp * 4);
    GL_CALL(glReadPixels(0, 0, Wp, Hp, GL_RGBA, GL_FLOAT, px.data()));
    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, old_fbo));
    GL_CALL(glDeleteFramebuffers(1, &fbo));

    // ============================================================
    // DUMP CONVENTION (must read):
    //  - shader coordinates: CSS pixels (scale=1!), origin at top-left
    //  - texture is stored with a Y FLIP (GL convention, origin at bottom):
    //    memory[y] = shader[Hp-1-y]
    //  - px_at() takes SHADER COORDINATES and flips them itself;
    //    all anchors/profiles below are given in shader coordinates.
    //  - alpha read: channel A; rgb — premultiplied (if premultiply
    //    is enabled in the shader) — compare levels by A.
    // ============================================================

    // anchor point in shader coordinates → address in memory (flip):
    auto px_at = [&](float xf, float yf) {
        int x = (int)(xf + 0.5f);
        int y = (int)(yf + 0.5f);
        x = std::clamp(x, 0, Wp - 1);
        y = std::clamp(y, 0, Hp - 1);
        int mem_y = y; // Hp - 1 - y;                       // flip: shader → memory
        const float* p = &px[((size_t)mem_y * Wp + x) * 4];
        char buf[64];
        snprintf(buf, sizeof(buf), "(%.3f,%.3f,%.3f,%.3f)", p[0], p[1], p[2], p[3]);
        return std::string(buf);
    };

    // alpha in shader coordinates (flip inside):
    auto alpha_at = [&](float xf, float yf) {
        int x = (int)(xf + 0.5f);
        int y = (int)(yf + 0.5f);
        x = std::clamp(x, 0, Wp - 1);
        y = std::clamp(y, 0, Hp - 1);
        int mem_y = y; // Hp - 1 - y;                       // flip: shader → memory
        return px[((size_t)(mem_y) * Wp + x) * 4 + 3];
    };

    // --- 1. uniforms: what is actually in the program ---
    float v1, v2[4], v3[4], v4[7], v5, v6, v7[4],
        v8[4], v9[4], v10[4], v11[4], v12[4]; // v13, v14;
    GL_CALL(glGetUniformfv(render_overlay_program, 1,  &v1));   // title_height (28 CSS)
    GL_CALL(glGetUniformfv(render_overlay_program, 2,   v2));   // titlebar border (0,0,1,0)
    GL_CALL(glGetUniformfv(render_overlay_program, 3,   v3));   // titlebar shadow(inset)
    GL_CALL(glGetUniformfv(render_overlay_program, 4,   v4));   // border (1,3,3,1)
    GL_CALL(glGetUniformfv(render_overlay_program, 5,  &v5));   // width
    GL_CALL(glGetUniformfv(render_overlay_program, 6,  &v6));   // height
    GL_CALL(glGetUniformfv(render_overlay_program, 7,   v7));   // radius (3,3,0,0)

    GL_CALL(glGetUniformfv(render_overlay_program, 8,   v8));   // titlebar background color
    GL_CALL(glGetUniformfv(render_overlay_program, 9,   v9));   // titlebar border color
    GL_CALL(glGetUniformfv(render_overlay_program, 10,  v10));  // titlebar shadow color
    GL_CALL(glGetUniformfv(render_overlay_program, 11,  v11));  // border color

    GL_CALL(glGetUniformfv(render_overlay_program, 12,  v12));  // u_margins (23,23,23-+8,23-8)
    //GL_CALL(glGetUniformfv(render_overlay_program, 13, &v13));  // offset_y (8/5)
    //GL_CALL(glGetUniformfv(render_overlay_program, 14, &v14));  // shadow_size (23)

    int v15;
    GL_CALL(glGetUniformiv(render_overlay_program, 15, &v15));  // shodow count
    float v16[4], v20[4];
    GL_CALL(glGetUniformfv(render_overlay_program, 16,  v16));  // border shadow
    GL_CALL(glGetUniformfv(render_overlay_program, 20,  v20));  // border shadow color

    // --- 2. shader geometry in CSS (for anchors) ---
    const float shadow_top = scale * v12[3];             //  23-8 = 15: top of the WINDOW
    const float win_l      = scale * v12[0];             //  23: left edge of the window
    const float win_r      = (float)Wp - scale * v12[1]; // 423: right edge of the window
    const float win_b      = (float)Hp - scale * v12[2]; // 374-(23+8) = 343: bottom of the window

    // --- 3. profiles: vertical at the left edge (x = win_l - 3, behind the ring) ---
    std::string prof;
    const float px_col = win_l - 3.0f;
    for (float y = 0; y < 60 && y < Hp; y += 4)      // top of the buffer (shader coords)
        prof += std::to_string((int)y) + ":" +
                std::to_string(alpha_at(px_col, y)) + " ";
    prof += " | ";
    for (float y = Hp - 70; y < Hp; y += 4)          // bottom of the buffer
        prof += std::to_string((int)y) + ":" +
                std::to_string(alpha_at(px_col, y)) + " ";

    // --- 4. horizontal profile (middle of the WINDOW height, not the buffer!) ---
    std::string hprof;
    const float prof_y = 0.5f * (shadow_top + win_b); // middle of the window
    for (float x = 0; x < win_l + 12 && x < Wp; x += 2)   // up to just past the window edge
        hprof += std::to_string((int)x) + ":" +
                 std::to_string(alpha_at(x, prof_y)) + " ";

    // --- 5. anchors in shader coordinates (CSS) ---
    LOGW("\n****** DUMP ******** (conv: CSS px, flip Y, alpha=ch A)",
         "\n  uni: width=", v5, " height=", v6,
         "\n       titlebar: h=", v1,
         " border=", v2[0], ',', v2[1], ',', v2[2], ',', v2[3],
         " shadow=", v3[0], ',', v3[1], ',', v3[2], ',', v3[3],
         "\n       decoration:",
         " border=", v4[0], ',', v4[1], ',', v4[2], ',', v4[3],
         " radius=", v7[0], ',', v7[1], ',', v7[2], ',', v7[3],
         "\n       margins: left=", v12[0], " right=", v12[1], " bottom=", v12[2], " top=", v12[3],
         "\n       box-shadow:  cnt=", v15,
         " off_x=", v16[0], " off_y=", v16[1], " σ=", v16[2]," spread=", v16[3],
         " r=", v20[0], " g=", v20[1], " b=", v20[2]," a=", v20[3],
         "\n  tex: Wp=", Wp, " Hp=", Hp,
         " window=[", (int)win_l, "..", (int)win_r, "] x [",
         (int)shadow_top, "..", (int)win_b, "]",
         "\n  ring_top=",     px_at(0.5f * (win_l + win_r), shadow_top - 1.0f),
         "\n  ring_left=",    px_at(win_l - 1.0f, 0.5f * (shadow_top + win_b)),
         "\n  ring_bottom=",  px_at(0.5f * (win_l + win_r), win_b + 1.0f),
         "\n  hilite=",       px_at(0.5f * (win_l + win_r), shadow_top + 0.5f),
         "\n  titlebar_mid=", px_at(0.5f * (win_l + win_r), 0.5f * (shadow_top + v1)),
         "\n  darkline=",     px_at(0.5f * (win_l + win_r), v1 - 0.5f),
         "\n  client=",       px_at(0.5f * (win_l + win_r), v1 + 4.0f),
         "\n  shadowL=",      px_at(win_l - 4.0f, 0.5f * (shadow_top + win_b)),
         "\n  shadowB=",      px_at(0.5f * (win_l + win_r), win_b + 4.0f),
         "\n  ds_map_check: d(x=win_l-3)≈", 3.0f, " expected cov≈0.43 at σ=15",
         "\n  V-prof(x=", (int)px_col, "): ", prof,
         "\n  H-prof(y=", (int)prof_y, "): ", hprof);
}

#endif

static const auto glUniformColor(GLint location, const color_t& c)
{
    return glUniform4f(location, c.r, c.g, c.b, c.a);
}

static const auto glUniformBorder(GLint location, const css::sides_t& b)
{
    return glUniform4f(location, b.top, b.right, b.bottom, b.left);
}

static const auto glUniformRadius(GLint location, const css::corners_t& r)
{
    return glUniform4f(location, r.top_left, r.top_right, r.bottom_right, r.bottom_left);
}

static const auto glUniformMargins(GLint location, const css::sides_t& m) // auto - c++20
{
    return glUniform4f(location, m.left, m.right, m.bottom, m.top); // structure fields in order
}

static const auto glUniformShadow(GLint location, const css::shadow_t& s)
{
    constexpr double π = 3.14159265358979323846; // Source - https://stackoverflow.com/a/49778398
    float r = s.blur_radius;
    float σ = (r * 3.0 * std::sqrt(2.0 * π) / 4.0) * 0.5;
    return glUniform4f(location, s.x_offset, s.y_offset, σ, s.spread_radius);
}

void shader_t::upload_box_shadow_static(GLuint program) // TODO, const PluginSettings& s)
{
    GL_CALL(glUseProgram(program));
#ifdef __DEBUG_SHADER__
    LOGW("upload_box_shadow_static()");
#endif
    // theme geometry (independent of the window)
    const auto& tb{ theme_t::titlebar };
    // titlebar border width (T,R,B,L): 0 0 1px 0
    GL_CALL(glUniformBorder(2,  tb.border.width));
    // TODO Active/backdrop
    if ((tb.shadows.size() == 1) && tb.shadows[0].is_inset)
    {
        GL_CALL(glUniformShadow(3, tb.shadows[0]));
        GL_CALL(glUniformColor(10, tb.shadows[0].color));
    } else {
        LOGW("Non-inset or multiple titlebar shadows are not supported");
        GL_CALL(glUniform4f(3,  0.0f, 0.0f, 0.0f, 0.0f));
        GL_CALL(glUniform4f(10, 0.0f, 0.0f, 0.0f, 0.0f));
    }

    GL_CALL(glUniformMargins(12, theme_t::gtk_margins));     // window margins
    // GL_CALL(glUniform1f(13, theme_t::deco_params.size));  // shadow width    = 23
}

void shader_t::update_gtk(const wf::scene::render_instruction_t& data,
                          const wf::geometry_t& rectangle, float scale,
                          bool activated, double title_height, bool render_decorations)
{
    if ((rectangle.width > 0) && (rectangle.height > 0))
    {
        wf::gles::run_in_context([&]
        {
            wf::gles::bind_render_buffer(data.target);
            // texture — physical: dimensions include scale
            // same floor(+0.5) contract
            const int W = px_round<int>(rectangle.width  * scale);
            const int H = px_round<int>(rectangle.height * scale);
            if ((W != px_width) || (H != px_height))
            {
                px_width  = W;
                px_height = H;
                recreate_textures();
            }
            GL_CALL(glActiveTexture(GL_TEXTURE0 + 0));
            GL_CALL(glBindTexture(GL_TEXTURE_2D, texture));

// const GLenum f = (scale == data.target.scale) ? GL_NEAREST : GL_LINEAR;
// GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
// GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));

            GL_CALL(glBindImageTexture(0, texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F));

            GL_CALL(glUseProgram(render_overlay_program));

            // Parameter breakdown (magic values)
            // (unit)    — image unit number,
            // (texture) — texture name, and name 0 in GL means "empty" (like nullptr) for unbind
            // (level)   — glTexStorage2D(…, 1, …) allocated exactly one level.
            // (layered) — must be FALSE for 2D textures
            // (layer)   — irrelevant for 2D textures
            // (access)  — write/read; GL_WRITE_ONLY is write-only
            // (format)  — byte layout seen by imageStore/imageLoad: GL_RGBA32F
            //             4 channels × 32-bit float = 16 bytes per texel.
            //             It participates in the "consistency triangle",
            //             which must line up in exactly three places:
            //             glTexStorage2D(..., GL_RGBA32F, ...) ↔ this call ↔ layout(rgba32f) in the shader.
            // GL_CALL(glBindImageTexture(0, texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F));

            // per-frame uniforms (CSS, scale will be applied in the shader)
            GL_CALL(glUniform1f(1,  title_height)); // titlebar_h = 28
            GL_CALL(glUniform1i(5,  W));
            GL_CALL(glUniform1i(6,  H));

            // palette
            const auto& tb{ theme_t::titlebar };
            const auto& b{ activated ? tb.border : tb.border_backdrop };
            const auto& bg{ activated ? tb.background : tb.background_backdrop };

            GL_CALL(glUniformColor(8,  bg.color));
            GL_CALL(glUniformColor(9,  b.color));     // Titlebar border color
            if (bg.with_gradient && bg.stops.size() == 2) {
                // other directions are not supported and should be abandoned durions settings loading
                if (bg.direction == css::gradient_direction_t::to_top) {
                    GL_CALL(glUniformColor(13, bg.stops[0].color)); // titlebar bottom gradient
                    GL_CALL(glUniformColor(14, bg.stops[1].color)); // titlebar top gradient
                } else {
                    GL_CALL(glUniformColor(13, bg.stops[1].color)); // titlebar bottom gradient
                    GL_CALL(glUniformColor(14, bg.stops[0].color)); // titlebar top gradient
                }
            } else { // Transparent gradient colors
                GL_CALL(glUniform4f(13, 0.0f, 0.0f, 0.0f, 0.0f));
                GL_CALL(glUniform4f(14, 0.0f, 0.0f, 0.0f, 0.0f));
            }
            if (render_decorations) {
                // display borders and shadows
                const auto& d{ activated ? theme_t::decoration
                                         : theme_t::decoration_backdrop };
                GL_CALL(glUniformBorder(4, d.border.width));
                GL_CALL(glUniformRadius(7, d.border.radius));
                GL_CALL(glUniformColor(11, d.border.color)); // Frame border color

                GLint j = 0;
                for (size_t i = 0; i < d.shadows.size(); i++) {

                    const auto& s{ d.shadows[i] };
                    if (s.color.a > 0) {
                        GL_CALL(glUniformShadow(16 + j, s));
                        GL_CALL(glUniformColor(20 + j, s.color));
                        if (++j == 4) break;
                    }
                    GL_CALL(glUniform1i(15, j));
                }
            } else {
                GL_CALL(glUniformBorder(4, {0,0,0,0}));
                // TODO check with GTK CSD
                GL_CALL(glUniformRadius(7, {0,0,0,0}));
                GL_CALL(glUniform1i(15, 0));
            }

            GL_CALL(glUniform1f(25, scale));

            GL_CALL(glDispatchCompute(round_up_div(W, 16),
                                      round_up_div(H, 16), 1));

            GL_CALL(glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT |
                                    GL_SHADER_IMAGE_ACCESS_BARRIER_BIT));

#ifdef __DEBUG_SHADER_RENDER__

static bool dumped = false;
if (!dumped && activated) {
    dumped = true;
    LOGW(fmt_geom(rectangle, "render_texture(): rectangle"), fmt_geom(data.target.geometry, " target"), " target scale=", data.target.scale,
    "\n\ttitle_height=", title_height);
    dump_box_shadow_shader(W, H, scale);
}

#endif
            GL_CALL(glBindImageTexture(0, 0, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F));

            GL_CALL(glUseProgram(0));
        });
    }
}

void shader_t::update_effects(const wf::scene::render_instruction_t& data,
                              const wf::geometry_t& rectangle, float scale,
                              bool activated, std::optional<wf::pointf_t> pt,
                              int title_height, int border_size, int shadow_size)
{
	bool fluid = (theme_t::effect_type == effect_t::smoke) || (theme_t::effect_type == effect_t::ink);

    if ((rectangle.width <= 0) || (rectangle.height <= 0))
    {
        return;
    }
    // pixdecor shaders doesn't support scale yet
    scale = 1.0;
    constexpr int diffuse_iterations = 2;

    wf::gles::run_in_context([&]
    {
        wf::gles::bind_render_buffer(data.target);

        const int W = px_round<int>(scale * rectangle.width);
        const int H = px_round<int>(scale * rectangle.height);
        if ((W != px_width) || (H != px_height))
        {
            px_width  = W;
            px_height = H;
            recreate_textures();
        }
        const double px_title_height = scale * title_height;
        const double px_border_size  = scale * border_size;
        const double px_shadow_size  = scale * shadow_size;
        const double px_radius = 0.5 * px_shadow_size;

        GL_CALL(glActiveTexture(GL_TEXTURE0 + 0));
        GL_CALL(glBindTexture(GL_TEXTURE_2D, texture));
        // Bind texture #0 in read/write mode
        GL_CALL(glBindImageTexture(0, texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F));

        if (fluid)
        {
            const wf::geometry_t nonshadow_rect = wf::geometry_t{
                .x      = static_cast<double>(px_shadow_size),
                .y      = static_cast<double>(px_shadow_size),
                .width  = static_cast<double>(W - 2 * px_shadow_size),
                .height = static_cast<double>(H - 2 * px_shadow_size)};

            wf::geometry_t client_rect = {
                .x      = static_cast<double>(px_shadow_size + px_border_size),
                .y      = static_cast<double>(px_shadow_size + px_border_size + px_title_height),
                .width  = static_cast<double>(W - 2 * px_border_size - 2 * px_shadow_size),
                .height = static_cast<double>(H - 2 * px_border_size - 2 * px_shadow_size - px_title_height)};

            wf::region_t border_region = wf::to_integer_box(nonshadow_rect);
            border_region ^= wf::to_integer_box(client_rect);
            border_region.expand_edges(scale);
            border_region &= wf::to_integer_box(nonshadow_rect);

            wf::point_t point;
            // gate: check BEFORE any conversion
            if (pt) {
                point = { px_round<int>(scale * pt->x),
                          px_round<int>(scale * pt->y) };
            } else {
                // the shader protocol — the only place where it lives
                point = { INT_MIN, INT_MIN };
            }
            GL_CALL(glActiveTexture(GL_TEXTURE0 + 1));
            GL_CALL(glBindTexture(GL_TEXTURE_2D, b0u));
            GL_CALL(glBindImageTexture(1, b0u, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F));
            GL_CALL(glActiveTexture(GL_TEXTURE0 + 2));
            GL_CALL(glBindTexture(GL_TEXTURE_2D, b0v));
            GL_CALL(glBindImageTexture(2, b0v, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F));
            GL_CALL(glActiveTexture(GL_TEXTURE0 + 3));
            GL_CALL(glBindTexture(GL_TEXTURE_2D, b0d));
            GL_CALL(glBindImageTexture(3, b0d, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F));
            GL_CALL(glActiveTexture(GL_TEXTURE0 + 4));
            GL_CALL(glBindTexture(GL_TEXTURE_2D, b1u));
            GL_CALL(glBindImageTexture(4, b1u, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F));
            GL_CALL(glActiveTexture(GL_TEXTURE0 + 5));
            GL_CALL(glBindTexture(GL_TEXTURE_2D, b1v));
            GL_CALL(glBindImageTexture(5, b1v, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F));
            GL_CALL(glActiveTexture(GL_TEXTURE0 + 6));
            GL_CALL(glBindTexture(GL_TEXTURE_2D, b1d));
            GL_CALL(glBindImageTexture(6, b1d, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F));

            if ((point.x >= px_radius) && (point.y >= px_radius))
            {
                GL_CALL(glUseProgram(motion_program));
                // upload stuff
                GL_CALL(glUniform1i(1, px_title_height + px_border_size + px_shadow_size));
                GL_CALL(glUniform1i(2, px_border_size + px_shadow_size));
                GL_CALL(glUniform1i(3, point.x));
                GL_CALL(glUniform1i(4, point.y));
                GL_CALL(glUniform1i(5, W));
                GL_CALL(glUniform1i(6, H));
                GL_CALL(glUniform1i(7, random()));
                GL_CALL(glUniform1i(8, random()));
                GL_CALL(glUniform1i(9, px_shadow_size));
                GL_CALL(glDispatchCompute(1, 1, 1));
                GL_CALL(glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT));
            }
            const wf::dimensions_t dims{ W, H };
            for (int k = 0; k < diffuse_iterations; k++)
            {
                render_region(diffuse1_program, border_region, dims);
            }

            render_region(project1_program, border_region, dims);
            for (int k = 0; k < diffuse_iterations; k++)
            {
                render_region(project2_program, border_region, dims);
            }

            render_region(project3_program, border_region, dims);
            render_region(advect1_program, border_region, dims);
            render_region(project4_program, border_region, dims);
            for (int k = 0; k < diffuse_iterations; k++)
            {
                render_region(project5_program, border_region, dims);
            }

            render_region(project6_program, border_region, dims);
            for (int k = 0; k < diffuse_iterations; k++)
            {
                render_region(diffuse2_program, border_region, dims);
            }

            render_region(advect2_program, border_region, dims);

            GL_CALL(glUseProgram(render_program));

            wf::color_t decoration_color = theme_t::decoration_color(activated);

            GL_CALL(glUniform1i(4, theme_t::is_ink_effect()));
            GL_CALL(glUniformColor(8, theme_t::effect_color));
            GL_CALL(glUniformColor(9, decoration_color));

            dispatch_region(border_region);

        } else if (theme_t::effect_type != effect_t::none)
        {
            GL_CALL(glUseProgram(render_program));

            GL_CALL(glUniform1i(1, px_title_height + px_border_size + px_shadow_size));
            GL_CALL(glUniform1i(2, px_border_size + px_shadow_size));
            GL_CALL(glUniform1i(5, W));
            GL_CALL(glUniform1i(6, H));
            GL_CALL(glUniform1i(7, px_shadow_size));
            GL_CALL(glUniform1f(9,
                theme_t::effect_animation_enabled ? ((wf::get_current_time() & 0x5FFFFFFF) / 30.0) : 0.0));
            GL_CALL(glDispatchCompute(W / 15, H / 15, 1));
            // Or ?
            //GL_CALL(glDispatchCompute(round_up_div(W, 16),
            //                          round_up_div(H, 16), 1));
            GL_CALL(glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT));

        } else if (theme_t::overlay_engine != overlay_t::none)
        {
            // "Fill" the border of the texture with color
            GLuint fb;
            wf::color_t decoration_color = theme_t::decoration_color(activated);
            GL_CALL(glGenFramebuffers(1, &fb));
            GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, fb));
            GL_CALL(glActiveTexture(GL_TEXTURE0 + 0));
            GL_CALL(glBindTexture(GL_TEXTURE_2D, texture));
            GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                           GL_TEXTURE_2D, texture, 0));
            OpenGL::clear(decoration_color, GL_COLOR_BUFFER_BIT);
            GL_CALL(glDeleteFramebuffers(1, &fb));
        }

        if (theme_t::overlay_engine != overlay_t::none)
        {
            GL_CALL(glUseProgram(render_overlay_program));

            GL_CALL(glUniform1i(1, px_shadow_size + px_border_size + px_title_height));
            GL_CALL(glUniform1i(2, px_shadow_size + px_border_size));
            GL_CALL(glUniform1i(5, W));
            GL_CALL(glUniform1i(6, H));

            if (theme_t::overlay_engine == overlay_t::rounded_corners)
            {
                GL_CALL(glUniform1i(7, px_round<int>(scale * theme_t::corner_radius)));
                GL_CALL(glUniform1i(8, px_round<int>(scale * theme_t::blur_radius)));
                GL_CALL(glUniformColor(9, theme_t::shadow_color)); // Frame border color
            }

            GL_CALL(glDispatchCompute(round_up_div(W, 16),
                                      round_up_div(H, 16), 1));
            GL_CALL(glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT));
        }
        // Unbind texture #0
        GL_CALL(glBindImageTexture(0, 0, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F));

        GL_CALL(glUseProgram(0));
    });
}

void shader_t::effect_changed()
{
    create_programs();
    wf::gles::run_in_context([&]
    {
        recreate_textures();
    });
}

} // namespace wf::pixdecor

