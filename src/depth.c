#include "private.h"

typedef struct vs_uniforms_t {
    mat4 mat_vp;
} vs_uniforms_t;

sg_pipeline init_depth_pipeline(void) {
    /* create an instancing shader */
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vs.uniform_blocks = {
            [0] = {
                .size = sizeof(vs_uniforms_t),
                .uniforms = {
                    [0] = { .name="u_mat_vp", .type=SG_UNIFORMTYPE_MAT4 }
                },
            }
        },
        .vs.source =
            "#version 330\n"
            "uniform mat4 u_mat_vp;\n"
            "layout(location=0) in vec4 v_position;\n"
            "layout(location=1) in mat4 i_mat_m;\n"
            "void main() {\n"
            "  gl_Position = u_mat_vp * i_mat_m * v_position;\n"
            "}\n",
        .fs.source =
            "#version 330\n"
            "out vec4 frag_color;\n"
            "void main() {\n"
            "  frag_color = vec4(0.0, 0.0, 0.0, 0.0);\n"
            "}\n"
    });

    return sg_make_pipeline(&(sg_pipeline_desc){
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .layout = {
            .buffers = {
                [1] = { .stride = 64, .step_func=SG_VERTEXSTEP_PER_INSTANCE }
            },

            .attrs = {
                /* Static geometry */
                [0] = { .buffer_index=0, .offset=0,  .format=SG_VERTEXFORMAT_FLOAT3 },
         
                /* Matrix (per instance) */
                [1] = { .buffer_index=1, .offset=0,  .format=SG_VERTEXFORMAT_FLOAT4 },
                [2] = { .buffer_index=1, .offset=16, .format=SG_VERTEXFORMAT_FLOAT4 },
                [3] = { .buffer_index=1, .offset=32, .format=SG_VERTEXFORMAT_FLOAT4 },
                [4] = { .buffer_index=1, .offset=48, .format=SG_VERTEXFORMAT_FLOAT4 }
            }
        },
        .blend = {
            .color_format = SG_PIXELFORMAT_RGBA8,
            .depth_format = SG_PIXELFORMAT_DEPTH
        },
        .depth_stencil = {
            .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL,
            .depth_write_enabled = true
        },
        .rasterizer.cull_mode = SG_CULLMODE_BACK
    });
}

sokol_offscreen_pass_t sokol_init_depth_pass(
    int32_t w, 
    int32_t h) 
{
    sg_image color_target = sokol_target_rgba8(w, h);
    sg_image depth_target = sokol_target_depth(w, h);
    ecs_rgb_t background_color = {0};

    return (sokol_offscreen_pass_t){
        .pass_action = sokol_clear_action(background_color, false, true),
        .pass = sg_make_pass(&(sg_pass_desc){
            .color_attachments[0].image = color_target,
            .depth_stencil_attachment.image = depth_target
        }),
        .pip = init_depth_pipeline(),
        .color_target = color_target,
        .depth_target = depth_target,
    };   
}

static
void init_uniforms(
    sokol_render_state_t *state,
    vs_uniforms_t *vs_out)
{
    vec3 eye = {0, 0, -2.0};
    vec3 center = {0.0, 0.0, 0.0};
    vec3 up = {0.0, 1.0, 0.0};

    mat4 mat_p;
    mat4 mat_v;

    /* Compute perspective & lookat matrix */
    if (state->camera) {
        EcsCamera cam = *state->camera;
        glm_perspective(cam.fov, state->aspect, cam.near, cam.far, mat_p);
        glm_lookat(cam.position, cam.lookat, cam.up, mat_v);
    } else {
        glm_perspective(30, state->aspect, 0.5, 100.0, mat_p);
        glm_lookat(eye, center, up, mat_v);
    }

    /* Compute view/projection matrix */
    glm_mat4_mul(mat_p, mat_v, vs_out->mat_vp);
}

static
void draw_instances(
    SokolGeometry *geometry,
    sokol_instances_t *instances)
{
    if (!instances->instance_count) {
        return;
    }

    sg_bindings bind = {
        .vertex_buffers = {
            [0] = geometry->vertex_buffer,
            [1] = instances->transform_buffer
        },
        .index_buffer = geometry->index_buffer
    };

    sg_apply_bindings(&bind);
    sg_draw(0, geometry->index_count, instances->instance_count);
}

void sokol_run_depth_pass(
    sokol_offscreen_pass_t *pass,
    sokol_render_state_t *state,
    sokol_vs_materials_t *mat_u)
{
    vs_uniforms_t vs_u;
    init_uniforms(state, &vs_u);

    /* Render to offscreen texture so screen-space effects can be applied */
    sg_begin_pass(pass->pass, &pass->pass_action);
    sg_apply_pipeline(pass->pip);

    sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &vs_u, sizeof(vs_uniforms_t));

    /* Loop geometry, render scene */
    ecs_iter_t qit = ecs_query_iter(state->q_scene);
    while (ecs_query_next(&qit)) {
        SokolGeometry *geometry = ecs_column(&qit, SokolGeometry, 1);

        int b;
        for (b = 0; b < qit.count; b ++) {
            draw_instances(&geometry[b], &geometry[b].solid);
            draw_instances(&geometry[b], &geometry[b].emissive);
        }
    }
    sg_end_pass();
}