//
// LICENSE:
//
// Copyright (c) 2016 -- 2020 Fabio Pellacini
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//

#include <yocto/yocto_color.h>
#include <yocto/yocto_commonio.h>
#include <yocto/yocto_parallel.h>
#include <yocto/yocto_sceneio.h>
#include <yocto/yocto_shape.h>
#include <yocto/yocto_trace.h>
#include <yocto_gui/yocto_imgui.h>
#include <yocto_gui/yocto_opengl.h>
using namespace yocto;

#include <deque>

namespace yocto {
void print_obj_camera(sceneio_camera* camera);
};  // namespace yocto

// Application scene
struct app_state {
  // loading options
  string filename  = "scene.json";
  string imagename = "out.png";
  string outname   = "out.json";
  string name      = "";

  // scene
  sceneio_scene*  ioscene  = new sceneio_scene{};
  trace_scene*    scene    = new trace_scene{};
  sceneio_camera* iocamera = nullptr;
  trace_camera*   camera   = nullptr;

  // rendering objects
  trace_lights* lights = new trace_lights{};
  trace_bvh*    bvh    = new trace_bvh{};

  // options
  trace_params params = {};

  // rendering state
  image<vec4f> render   = {};
  image<vec4f> display  = {};
  float        exposure = 0;

  // view scene
  ogl_image*       glimage  = new ogl_image{};
  ogl_image_params glparams = {};

  // editing
  sceneio_camera*      selected_camera      = nullptr;
  sceneio_instance*    selected_instance    = nullptr;
  sceneio_shape*       selected_shape       = nullptr;
  sceneio_material*    selected_material    = nullptr;
  sceneio_environment* selected_environment = nullptr;
  sceneio_texture*     selected_texture     = nullptr;

  // computation
  int          render_sample  = 0;
  int          render_counter = 0;
  trace_state* render_state   = new trace_state{};

  // loading status
  std::atomic<bool> ok           = false;
  std::future<void> loader       = {};
  string            status       = "";
  string            error        = "";
  std::atomic<int>  current      = 0;
  std::atomic<int>  total        = 0;
  string            loader_error = "";

  ~app_state() {
    if (render_state) trace_stop(render_state);
    delete render_state;
    delete bvh;
    delete lights;
    delete scene;
    delete ioscene;
    delete glimage;
  }
};

// Application state
struct app_states {
  // data
  vector<app_state*>     states   = {};
  app_state*             selected = nullptr;
  std::deque<app_state*> loading  = {};

  // default options
  trace_params params     = {};
  bool         add_skyenv = false;

  // cleanup
  ~app_states() {
    for (auto state : states) delete state;
  }
};

// Construct a scene from io
void init_scene(trace_scene* scene, sceneio_scene* ioscene,
    trace_camera*& camera, sceneio_camera* iocamera,
    progress_callback progress_cb = {}) {
  // handle progress
  auto progress = vec2i{
      0, (int)ioscene->cameras.size() + (int)ioscene->environments.size() +
             (int)ioscene->materials.size() + (int)ioscene->textures.size() +
             (int)ioscene->shapes.size() + (int)ioscene->instances.size()};

  auto camera_map     = unordered_map<sceneio_camera*, trace_camera*>{};
  camera_map[nullptr] = nullptr;
  for (auto iocamera : ioscene->cameras) {
    if (progress_cb)
      progress_cb("converting cameras", progress.x++, progress.y);
    auto camera          = add_camera(scene);
    camera->frame        = iocamera->frame;
    camera->lens         = iocamera->lens;
    camera->aspect       = iocamera->aspect;
    camera->film         = iocamera->film;
    camera->orthographic = iocamera->orthographic;
    camera->aperture     = iocamera->aperture;
    camera->focus        = iocamera->focus;
    camera_map[iocamera] = camera;
  }

  auto texture_map     = unordered_map<sceneio_texture*, trace_texture*>{};
  texture_map[nullptr] = nullptr;
  for (auto iotexture : ioscene->textures) {
    if (progress_cb)
      progress_cb("converting textures", progress.x++, progress.y);
    auto texture           = add_texture(scene);
    texture->hdr           = iotexture->hdr;
    texture->ldr           = iotexture->ldr;
    texture_map[iotexture] = texture;
  }

  auto material_map     = unordered_map<sceneio_material*, trace_material*>{};
  material_map[nullptr] = nullptr;
  for (auto iomaterial : ioscene->materials) {
    if (progress_cb)
      progress_cb("converting materials", progress.x++, progress.y);
    auto material              = add_material(scene);
    material->emission         = iomaterial->emission;
    material->color            = iomaterial->color;
    material->specular         = iomaterial->specular;
    material->roughness        = iomaterial->roughness;
    material->metallic         = iomaterial->metallic;
    material->ior              = iomaterial->ior;
    material->spectint         = iomaterial->spectint;
    material->coat             = iomaterial->coat;
    material->transmission     = iomaterial->transmission;
    material->translucency     = iomaterial->translucency;
    material->scattering       = iomaterial->scattering;
    material->scanisotropy     = iomaterial->scanisotropy;
    material->trdepth          = iomaterial->trdepth;
    material->opacity          = iomaterial->opacity;
    material->thin             = iomaterial->thin;
    material->emission_tex     = texture_map.at(iomaterial->emission_tex);
    material->color_tex        = texture_map.at(iomaterial->color_tex);
    material->specular_tex     = texture_map.at(iomaterial->specular_tex);
    material->metallic_tex     = texture_map.at(iomaterial->metallic_tex);
    material->roughness_tex    = texture_map.at(iomaterial->roughness_tex);
    material->transmission_tex = texture_map.at(iomaterial->transmission_tex);
    material->translucency_tex = texture_map.at(iomaterial->translucency_tex);
    material->spectint_tex     = texture_map.at(iomaterial->spectint_tex);
    material->scattering_tex   = texture_map.at(iomaterial->scattering_tex);
    material->coat_tex         = texture_map.at(iomaterial->coat_tex);
    material->opacity_tex      = texture_map.at(iomaterial->opacity_tex);
    material->normal_tex       = texture_map.at(iomaterial->normal_tex);
    material_map[iomaterial]   = material;
  }

  auto shape_map     = unordered_map<sceneio_shape*, trace_shape*>{};
  shape_map[nullptr] = nullptr;
  for (auto ioshape : ioscene->shapes) {
    if (progress_cb) progress_cb("converting shapes", progress.x++, progress.y);
    auto shape              = add_shape(scene);
    shape->points           = ioshape->points;
    shape->lines            = ioshape->lines;
    shape->triangles        = ioshape->triangles;
    shape->quads            = ioshape->quads;
    shape->quadspos         = ioshape->quadspos;
    shape->quadsnorm        = ioshape->quadsnorm;
    shape->quadstexcoord    = ioshape->quadstexcoord;
    shape->positions        = ioshape->positions;
    shape->normals          = ioshape->normals;
    shape->texcoords        = ioshape->texcoords;
    shape->colors           = ioshape->colors;
    shape->radius           = ioshape->radius;
    shape->tangents         = ioshape->tangents;
    shape->subdivisions     = ioshape->subdivisions;
    shape->catmullclark     = ioshape->catmullclark;
    shape->smooth           = ioshape->smooth;
    shape->displacement     = ioshape->displacement;
    shape->displacement_tex = texture_map.at(ioshape->displacement_tex);
    shape_map[ioshape]      = shape;
  }

  for (auto ioinstance : ioscene->instances) {
    if (progress_cb)
      progress_cb("converting instances", progress.x++, progress.y);
    auto instance      = add_instance(scene);
    instance->frame    = ioinstance->frame;
    instance->shape    = shape_map.at(ioinstance->shape);
    instance->material = material_map.at(ioinstance->material);
  }

  for (auto ioenvironment : ioscene->environments) {
    if (progress_cb)
      progress_cb("converting environments", progress.x++, progress.y);
    auto environment          = add_environment(scene);
    environment->frame        = ioenvironment->frame;
    environment->emission     = ioenvironment->emission;
    environment->emission_tex = texture_map.at(ioenvironment->emission_tex);
  }

  // done
  if (progress_cb) progress_cb("converting done", progress.x++, progress.y);

  // get camera
  camera = camera_map.at(iocamera);
}

void stop_display(app_state* app) {
  // stop render
  trace_stop(app->render_state);
}

void reset_display(app_state* app) {
  // stop render
  trace_stop(app->render_state);

  // start render
  app->status         = "render";
  app->render_counter = 0;
  trace_start(
      app->render_state, app->scene, app->camera, app->bvh, app->lights,
      app->params,
      [app](const string& message, int sample, int nsamples) {
        app->current = sample;
        app->total   = nsamples;
      },
      [app](const image<vec4f>& render, int current, int total) {
        if (current > 0) return;
        app->render  = render;
        app->display = tonemap_image(app->render, app->exposure);
      },
      [app](
          const image<vec4f>& render, int current, int total, const vec2i& ij) {
        app->render[ij]  = render[ij];
        app->display[ij] = tonemap(app->render[ij], app->exposure);
      });
}

void load_scene_async(app_states* apps, const string& filename,
    const string& camera_name = "", bool add_skyenv = false) {
  auto app       = apps->states.emplace_back(new app_state{});
  app->name      = path_filename(filename) + " [loading]";
  app->filename  = filename;
  app->imagename = replace_extension(filename, ".png");
  app->outname   = replace_extension(filename, ".edited.json");
  app->params    = apps->params;
  app->status    = "load";
  app->loader    = std::async(std::launch::async, [app, camera_name,
                                                   add_skyenv]() {
    auto progress_cb = [app](const string& message, int current, int total) {
      app->current = current;
      app->total   = total;
    };
    if (!load_scene(
            app->filename, app->ioscene, app->loader_error, progress_cb))
      return;
    app->current = 1;
    app->total   = 1;
    if (add_skyenv) add_sky(app->ioscene);
    app->iocamera = get_camera(app->ioscene, camera_name);
    init_scene(
        app->scene, app->ioscene, app->camera, app->iocamera, progress_cb);
    tesselate_shapes(app->scene, progress_cb);
    init_bvh(app->bvh, app->scene, app->params);
    init_lights(app->lights, app->scene, app->params);
    if (app->lights->lights.empty() && is_sampler_lit(app->params)) {
      app->params.sampler = trace_sampler_type::eyelight;
    }
  });
  apps->loading.push_back(app);
  if (!apps->selected) apps->selected = app;
}

bool draw_widgets(
    gui_window* win, sceneio_scene* ioscene, sceneio_camera* iocamera) {
  if (!iocamera) return false;
  auto edited = 0;
  draw_label(win, "name", iocamera->name);
  edited += draw_slider(win, "frame.x", iocamera->frame.x, -1, 1);
  edited += draw_slider(win, "frame.y", iocamera->frame.y, -1, 1);
  edited += draw_slider(win, "frame.z", iocamera->frame.z, -1, 1);
  edited += draw_slider(win, "frame.o", iocamera->frame.o, -10, 10);
  edited += draw_checkbox(win, "ortho", iocamera->orthographic);
  edited += draw_slider(win, "lens", iocamera->lens, 0.01f, 1);
  edited += draw_slider(win, "film", iocamera->film, 0.01f, 0.1f);
  edited += draw_slider(win, "focus", iocamera->focus, 0.01f, 1000);
  edited += draw_slider(win, "aperture", iocamera->aperture, 0, 5);
  auto from         = iocamera->frame.o,
       to           = iocamera->frame.o - iocamera->focus * iocamera->frame.z;
  auto from_changed = draw_slider(win, "!!from", from, -10, 10);
  auto to_changed   = draw_slider(win, "!!to", to, -10, 10);
  if (from_changed || to_changed) {
    iocamera->frame = lookat_frame(from, to, {0, 1, 0});
    iocamera->focus = length(from - to);
    edited += 1;
  }
  return edited;
}

bool draw_widgets(
    gui_window* win, sceneio_scene* ioscene, sceneio_texture* iotexture) {
  if (!iotexture) return false;
  auto edited = 0;
  draw_label(win, "name", iotexture->name);
  draw_label(win, "hdr",
      std::to_string(iotexture->hdr.width()) + " x " +
          std::to_string(iotexture->hdr.height()));
  draw_label(win, "ldr",
      std::to_string(iotexture->ldr.width()) + " x " +
          std::to_string(iotexture->ldr.height()));
  // TODO(fabio): load texture
  return edited;
}

bool draw_widgets(
    gui_window* win, sceneio_scene* ioscene, sceneio_material* iomaterial) {
  if (!iomaterial) return false;
  auto edited = 0;
  draw_label(win, "name", iomaterial->name);
  edited += draw_hdrcoloredit(win, "emission", iomaterial->emission);
  edited += draw_coloredit(win, "color", iomaterial->color);
  edited += draw_slider(win, "opacity", iomaterial->opacity, 0, 1);
  edited += draw_slider(win, "metallic", iomaterial->metallic, 0, 1);
  edited += draw_slider(win, "roughness", iomaterial->roughness, 0, 1);
  edited += draw_slider(win, "specular", iomaterial->specular, 0, 1);
  edited += draw_slider(win, "coat", iomaterial->coat, 0, 1);
  edited += draw_slider(win, "transmission", iomaterial->transmission, 0, 1);
  edited += draw_slider(win, "translucency", iomaterial->translucency, 0, 1);
  edited += draw_coloredit(win, "spectint", iomaterial->spectint);
  edited += draw_checkbox(win, "thin", iomaterial->thin);
  edited += draw_coloredit(win, "scattering", iomaterial->scattering);
  edited += draw_slider(win, "trdepth", iomaterial->trdepth, 0, 1);
  edited += draw_slider(win, "scanisotropy", iomaterial->scanisotropy, -1, 1);
  edited += draw_combobox(
      win, "emission_tex", iomaterial->emission_tex, ioscene->textures, true);
  edited += draw_combobox(
      win, "color_tex", iomaterial->color_tex, ioscene->textures, true);
  edited += draw_combobox(
      win, "opacity_tex", iomaterial->opacity_tex, ioscene->textures, true);
  edited += draw_combobox(
      win, "metallic_tex", iomaterial->metallic_tex, ioscene->textures, true);
  edited += draw_combobox(
      win, "roughness_tex", iomaterial->roughness_tex, ioscene->textures, true);
  edited += draw_combobox(
      win, "specular_tex", iomaterial->specular_tex, ioscene->textures, true);
  edited += draw_combobox(win, "transmission_tex", iomaterial->transmission_tex,
      ioscene->textures, true);
  edited += draw_combobox(win, "translucency_tex", iomaterial->translucency_tex,
      ioscene->textures, true);
  edited += draw_combobox(win, "scattering_tex", iomaterial->scattering_tex,
      ioscene->textures, true);
  edited += draw_combobox(
      win, "spectint_tex", iomaterial->spectint_tex, ioscene->textures, true);
  edited += draw_combobox(
      win, "normal_tex", iomaterial->normal_tex, ioscene->textures, true);
  return edited;
}

bool draw_widgets(
    gui_window* win, sceneio_scene* ioscene, sceneio_shape* ioshape) {
  if (!ioshape) return false;
  auto edited = 0;
  draw_label(win, "name", ioshape->name);
  draw_label(win, "points", std::to_string(ioshape->points.size()));
  draw_label(win, "lines", std::to_string(ioshape->lines.size()));
  draw_label(win, "triangles", std::to_string(ioshape->triangles.size()));
  draw_label(win, "quads", std::to_string(ioshape->quads.size()));
  draw_label(win, "positions", std::to_string(ioshape->positions.size()));
  draw_label(win, "normals", std::to_string(ioshape->normals.size()));
  draw_label(win, "texcoords", std::to_string(ioshape->texcoords.size()));
  draw_label(win, "colors", std::to_string(ioshape->colors.size()));
  draw_label(win, "radius", std::to_string(ioshape->radius.size()));
  draw_label(win, "tangents", std::to_string(ioshape->tangents.size()));
  draw_label(win, "quads pos", std::to_string(ioshape->quadspos.size()));
  draw_label(win, "quads norm", std::to_string(ioshape->quadsnorm.size()));
  draw_label(
      win, "quads texcoord", std::to_string(ioshape->quadstexcoord.size()));
  edited += draw_slider(win, "subdivisions", ioshape->subdivisions, 0, 5);
  edited += draw_checkbox(win, "catmull-clark", ioshape->catmullclark);
  edited += draw_slider(win, "displacement", ioshape->displacement, 0, 1);
  edited += draw_combobox(win, "displacement_tex", ioshape->displacement_tex,
      ioscene->textures, true);
  return edited;
}

bool draw_widgets(
    gui_window* win, sceneio_scene* ioscene, sceneio_instance* ioobject) {
  if (!ioobject) return false;
  auto edited = 0;
  draw_label(win, "name", ioobject->name);
  edited += draw_slider(win, "frame.x", ioobject->frame.x, -1, 1);
  edited += draw_slider(win, "frame.y", ioobject->frame.y, -1, 1);
  edited += draw_slider(win, "frame.z", ioobject->frame.z, -1, 1);
  edited += draw_slider(win, "frame.o", ioobject->frame.o, -10, 10);
  edited += draw_combobox(win, "shape", ioobject->shape, ioscene->shapes);
  edited += draw_combobox(
      win, "material", ioobject->material, ioscene->materials);
  return edited;
}

bool draw_widgets(gui_window* win, sceneio_scene* ioscene,
    sceneio_environment* ioenvironment) {
  if (!ioenvironment) return false;
  auto edited = 0;
  draw_label(win, "name", ioenvironment->name);
  edited += draw_slider(win, "frame.x", ioenvironment->frame.x, -1, 1);
  edited += draw_slider(win, "frame.y", ioenvironment->frame.y, -1, 1);
  edited += draw_slider(win, "frame.z", ioenvironment->frame.z, -1, 1);
  edited += draw_slider(win, "frame.o", ioenvironment->frame.o, -10, 10);
  edited += draw_hdrcoloredit(win, "emission", ioenvironment->emission);
  edited += draw_combobox(win, "emission texture", ioenvironment->emission_tex,
      ioscene->textures, true);
  return edited;
}

template <typename T, typename T1>
T1* get_element(
    T* ioelement, const vector<T*>& ioelements, const vector<T1*>& elements) {
  if (!ioelement) return nullptr;
  for (auto pos = 0; pos < ioelements.size(); pos++) {
    if (ioelements[pos] == ioelement) return elements[pos];
  }
  print_fatal("element not found");
  return nullptr;
}

void draw_widgets(gui_window* win, app_states* apps, const gui_input& input) {
  static string load_path = "", save_path = "", error_message = "";
  if (draw_filedialog_button(win, "load", true, "load", load_path, false, "./",
          "", "*.json;*.obj;*.pbrt")) {
    load_scene_async(apps, load_path);
    load_path = "";
  }
  continue_line(win);
  if (draw_filedialog_button(win, "save", apps->selected && apps->selected->ok,
          "save", save_path, true, path_dirname(save_path),
          path_filename(save_path), "*.json;*.obj;*.pbrt")) {
    auto app     = apps->selected;
    app->outname = save_path;
    save_scene(app->outname, app->ioscene, app->error);
    save_path = "";
  }
  continue_line(win);
  if (draw_filedialog_button(win, "save image",
          apps->selected && apps->selected->ok, "save image", save_path, true,
          path_dirname(save_path), path_filename(save_path),
          "*.png;*.jpg;*.tga;*.bmp;*.hdr;*.exr")) {
    auto app     = apps->selected;
    app->outname = save_path;
    save_image(app->imagename, app->display, app->error);
    save_path = "";
  }
  continue_line(win);
  if (draw_button(win, "close", (bool)apps->selected)) {
    if (apps->selected->loader.valid()) return;
    delete apps->selected;
    apps->states.erase(
        std::find(apps->states.begin(), apps->states.end(), apps->selected));
    apps->selected = apps->states.empty() ? nullptr : apps->states.front();
  }
  continue_line(win);
  if (draw_button(win, "quit")) {
    set_close(win, true);
  }
  draw_combobox(win, "scene", apps->selected, apps->states, false);
  if (!apps->selected) return;
  draw_progressbar(win, apps->selected->status.c_str(), apps->selected->current,
      apps->selected->total);
  if (apps->selected->error != "") {
    draw_label(win, "error", apps->selected->error);
    return;
  }
  if (!apps->selected->ok) return;
  auto app = apps->selected;
  if (begin_header(win, "trace")) {
    auto edited = 0;
    if (draw_combobox(win, "camera", app->iocamera, app->ioscene->cameras)) {
      app->camera = get_element(
          app->iocamera, app->ioscene->cameras, app->scene->cameras);
      edited += 1;
    }
    auto& tparams = app->params;
    edited += draw_slider(win, "resolution", tparams.resolution, 180, 4096);
    edited += draw_slider(win, "nsamples", tparams.samples, 16, 4096);
    edited += draw_combobox(
        win, "tracer", (int&)tparams.sampler, trace_sampler_names);
    edited += draw_combobox(
        win, "false color", (int&)tparams.falsecolor, trace_falsecolor_names);
    edited += draw_slider(win, "nbounces", tparams.bounces, 1, 128);
    edited += draw_checkbox(win, "envhidden", tparams.envhidden);
    continue_line(win);
    edited += draw_checkbox(win, "filter", tparams.tentfilter);
    edited += draw_slider(win, "seed", (int&)tparams.seed, 0, 1000000);
    edited += draw_slider(win, "pratio", tparams.pratio, 1, 64);
    edited += draw_slider(win, "exposure", app->exposure, -5, 5);
    if (edited) reset_display(app);
    end_header(win);
  }
  if (begin_header(win, "inspect")) {
    draw_label(win, "scene", app->name);
    draw_label(win, "filename", app->filename);
    draw_label(win, "outname", app->outname);
    draw_label(win, "imagename", app->imagename);
    if (app->ok) {
      draw_label(win, "image",
          std::to_string(app->render.width()) + " x " +
              std::to_string(app->render.height()) + " @ " +
              std::to_string(app->render_sample));
      draw_slider(win, "zoom", app->glparams.scale, 0.1, 10);
      draw_checkbox(win, "zoom to fit", app->glparams.fit);
      continue_line(win);
      if (draw_button(win, "print cams")) {
        for (auto iocamera : app->ioscene->cameras) {
          print_obj_camera(iocamera);
        }
      }
      continue_line(win);
      if (draw_button(win, "print stats")) {
        for (auto stat : scene_stats(app->ioscene)) print_info(stat);
      }
      auto ij = image_coords(input.mouse_pos, app->glparams.center,
          app->glparams.scale, app->render.imsize());
      draw_dragger(win, "mouse", ij);
      if (ij.x >= 0 && ij.x < app->render.width() && ij.y >= 0 &&
          ij.y < app->render.height()) {
        draw_coloredit(win, "pixel", app->render[{ij.x, ij.y}]);
      } else {
        auto zero4f_ = zero4f;
        draw_coloredit(win, "pixel", zero4f_);
      }
    }
    end_header(win);
  }
  auto get_texture = [app](sceneio_texture* iotexture) {
    return get_element(iotexture, app->ioscene->textures, app->scene->textures);
  };
  if (!app->ioscene->cameras.empty() && begin_header(win, "cameras")) {
    draw_combobox(
        win, "camera##2", app->selected_camera, app->ioscene->cameras, true);
    if (draw_widgets(win, app->ioscene, app->selected_camera)) {
      stop_display(app);
      auto iocamera = app->selected_camera;
      auto camera   = get_element(
          iocamera, app->ioscene->cameras, app->scene->cameras);
      camera->frame        = iocamera->frame;
      camera->lens         = iocamera->lens;
      camera->aspect       = iocamera->aspect;
      camera->film         = iocamera->film;
      camera->orthographic = iocamera->orthographic;
      camera->aperture     = iocamera->aperture;
      camera->focus        = iocamera->focus;
      reset_display(app);
    }
    end_header(win);
  }
  if (!app->ioscene->environments.empty() &&
      begin_header(win, "environments")) {
    draw_combobox(win, "environment##2", app->selected_environment,
        app->ioscene->environments, true);
    if (draw_widgets(win, app->ioscene, app->selected_environment)) {
      stop_display(app);
      auto ioenvironment = app->selected_environment;
      auto environment   = get_element(
          ioenvironment, app->ioscene->environments, app->scene->environments);
      environment->frame        = ioenvironment->frame;
      environment->emission     = ioenvironment->emission;
      environment->emission_tex = get_texture(ioenvironment->emission_tex);
      init_lights(app->lights, app->scene, app->params);
      reset_display(app);
    }
    end_header(win);
  }
  if (!app->ioscene->instances.empty() && begin_header(win, "instances")) {
    draw_combobox(win, "instance##2", app->selected_instance,
        app->ioscene->instances, true);
    if (draw_widgets(win, app->ioscene, app->selected_instance)) {
      stop_display(app);
      auto ioinstance = app->selected_instance;
      auto instance   = get_element(
          ioinstance, app->ioscene->instances, app->scene->instances);
      instance->frame = ioinstance->frame;
      instance->shape = get_element(
          ioinstance->shape, app->ioscene->shapes, app->scene->shapes);
      instance->material = get_element(
          ioinstance->material, app->ioscene->materials, app->scene->materials);
      update_bvh(app->bvh, app->scene, {instance}, {}, app->params);
      reset_display(app);
    }
    end_header(win);
  }
  if (!app->ioscene->shapes.empty() && begin_header(win, "shapes")) {
    draw_combobox(
        win, "shape##2", app->selected_shape, app->ioscene->shapes, true);
    if (draw_widgets(win, app->ioscene, app->selected_shape)) {
      stop_display(app);
      auto ioshape = app->selected_shape;
      auto shape   = get_element(
          ioshape, app->ioscene->shapes, app->scene->shapes);
      shape->points    = ioshape->points;
      shape->lines     = ioshape->lines;
      shape->triangles = ioshape->triangles;
      shape->quads     = ioshape->quads;
      shape->positions = ioshape->positions;
      shape->normals   = ioshape->normals;
      shape->texcoords = ioshape->texcoords;
      shape->colors    = ioshape->colors;
      shape->radius    = ioshape->radius;
      shape->tangents  = ioshape->tangents;
      update_bvh(app->bvh, app->scene, {}, {shape}, app->params);
      reset_display(app);
    }
    end_header(win);
  }
  if (!app->ioscene->materials.empty() && begin_header(win, "materials")) {
    draw_combobox(win, "material##2", app->selected_material,
        app->ioscene->materials, true);
    if (draw_widgets(win, app->ioscene, app->selected_material)) {
      stop_display(app);
      auto iomaterial = app->selected_material;
      auto material   = get_element(
          iomaterial, app->ioscene->materials, app->scene->materials);
      material->emission         = iomaterial->emission;
      material->color            = iomaterial->color;
      material->specular         = iomaterial->specular;
      material->roughness        = iomaterial->roughness;
      material->metallic         = iomaterial->metallic;
      material->ior              = iomaterial->ior;
      material->spectint         = iomaterial->spectint;
      material->coat             = iomaterial->coat;
      material->transmission     = iomaterial->transmission;
      material->translucency     = iomaterial->translucency;
      material->scattering       = iomaterial->scattering;
      material->scanisotropy     = iomaterial->scanisotropy;
      material->trdepth          = iomaterial->trdepth;
      material->opacity          = iomaterial->opacity;
      material->thin             = iomaterial->thin;
      material->emission_tex     = get_texture(iomaterial->emission_tex);
      material->color_tex        = get_texture(iomaterial->color_tex);
      material->specular_tex     = get_texture(iomaterial->specular_tex);
      material->metallic_tex     = get_texture(iomaterial->metallic_tex);
      material->roughness_tex    = get_texture(iomaterial->roughness_tex);
      material->transmission_tex = get_texture(iomaterial->transmission_tex);
      material->translucency_tex = get_texture(iomaterial->translucency_tex);
      material->spectint_tex     = get_texture(iomaterial->spectint_tex);
      material->scattering_tex   = get_texture(iomaterial->scattering_tex);
      material->coat_tex         = get_texture(iomaterial->coat_tex);
      material->opacity_tex      = get_texture(iomaterial->opacity_tex);
      material->normal_tex       = get_texture(iomaterial->normal_tex);
      init_lights(app->lights, app->scene, app->params);
      reset_display(app);
    }
    end_header(win);
  }
  if (!app->ioscene->textures.empty() && begin_header(win, "textures")) {
    draw_combobox(win, "textures##2", app->selected_texture,
        app->ioscene->textures, true);
    if (draw_widgets(win, app->ioscene, app->selected_texture)) {
      stop_display(app);
      auto iotexture = app->selected_texture;
      auto texture   = get_element(
          iotexture, app->ioscene->textures, app->scene->textures);
      texture->hdr = iotexture->hdr;
      texture->ldr = iotexture->ldr;
      reset_display(app);
    }
    end_header(win);
  }
}

void draw(gui_window* win, app_states* apps, const gui_input& input) {
  if (!apps->selected || !apps->selected->ok) return;
  auto app                  = apps->selected;
  app->glparams.window      = input.window_size;
  app->glparams.framebuffer = input.framebuffer_viewport;
  if (!is_initialized(app->glimage)) init_image(app->glimage);
  if (!app->render_counter) set_image(app->glimage, app->display, false, false);
  std::tie(app->glparams.center, app->glparams.scale) = camera_imview(
      app->glparams.center, app->glparams.scale, app->display.imsize(),
      app->glparams.window, app->glparams.fit);
  draw_image(app->glimage, app->glparams);
  app->render_counter++;
  if (app->render_counter > 10) app->render_counter = 0;
}

void update(gui_window* win, app_states* apps) {
  auto is_ready = [](const std::future<void>& result) -> bool {
    return result.valid() && result.wait_for(std::chrono::microseconds(0)) ==
                                 std::future_status::ready;
  };

  while (!apps->loading.empty()) {
    auto app = apps->loading.front();
    if (!is_ready(app->loader)) break;
    apps->loading.pop_front();
    app->loader.get();
    if (app->loader_error.empty()) {
      app->status = "done";
      app->ok     = true;
      reset_display(app);
    } else {
      app->error  = app->loader_error;
      app->status = "error";
    }
  }
}

int main(int argc, const char* argv[]) {
  // application
  auto apps_guard  = std::make_unique<app_states>();
  auto apps        = apps_guard.get();
  auto filenames   = vector<string>{};
  auto add_skyenv  = false;
  auto camera_name = ""s;

  // parse command line
  auto cli = make_cli("yscnitrace", "progressive path tracing");
  add_option(cli, "--camera", camera_name, "Camera name.");
  add_option(
      cli, "--resolution,-r", apps->params.resolution, "Image resolution.");
  add_option(cli, "--samples,-s", apps->params.samples, "Number of samples.");
  add_option(cli, "--tracer,-t", apps->params.sampler, "Tracer type.",
      trace_sampler_names);
  add_option(cli, "--falsecolor,-F", apps->params.falsecolor,
      "Tracer false color type.", trace_falsecolor_names);
  add_option(
      cli, "--bounces,-b", apps->params.bounces, "Maximum number of bounces.");
  add_option(cli, "--clamp", apps->params.clamp, "Final pixel clamping.");
  add_option(
      cli, "--filter/--no-filter", apps->params.tentfilter, "Filter image.");
  add_option(cli, "--env-hidden/--no-env-hidden", apps->params.envhidden,
      "Environments are hidden in renderer");
  add_option(cli, "--bvh", apps->params.bvh, "Bvh type", trace_bvh_names);
  add_option(cli, "--skyenv/--no-skyenv", add_skyenv, "Add sky envmap");
  add_option(cli, "scenes", filenames, "Scene filenames", true);
  parse_cli(cli, argc, argv);

  // loading images
  for (auto filename : filenames)
    load_scene_async(apps, filename, camera_name, add_skyenv);

  // callbacks
  auto callbacks     = gui_callbacks{};
  callbacks.clear_cb = [apps](gui_window* win, const gui_input& input) {
    for (auto app : apps->states) clear_image(app->glimage);
  };
  callbacks.draw_cb = [apps](gui_window* win, const gui_input& input) {
    draw(win, apps, input);
  };
  callbacks.widgets_cb = [apps](gui_window* win, const gui_input& input) {
    draw_widgets(win, apps, input);
  };
  callbacks.drop_cb = [apps](gui_window* win, const vector<string>& paths,
                          const gui_input& input) {
    for (auto& path : paths) load_scene_async(apps, path);
  };
  callbacks.update_cb = [apps](gui_window* win, const gui_input& input) {
    update(win, apps);
  };
  callbacks.uiupdate_cb = [apps](gui_window* win, const gui_input& input) {
    if (!apps->selected) return;
    auto app = apps->selected;

    // handle mouse and keyboard for navigation
    if ((input.mouse_left || input.mouse_right) && !input.modifier_alt &&
        !input.widgets_active) {
      auto dolly  = 0.0f;
      auto pan    = zero2f;
      auto rotate = zero2f;
      if (input.mouse_left && !input.modifier_shift)
        rotate = (input.mouse_pos - input.mouse_last) / 100.0f;
      if (input.mouse_right)
        dolly = (input.mouse_pos.x - input.mouse_last.x) / 100.0f;
      if (input.mouse_left && input.modifier_shift)
        pan = (input.mouse_pos - input.mouse_last) * app->iocamera->focus /
              200.0f;
      pan.x = -pan.x;
      stop_display(app);
      std::tie(app->iocamera->frame, app->iocamera->focus) = camera_turntable(
          app->iocamera->frame, app->iocamera->focus, rotate, dolly, pan);
      app->camera->frame = app->iocamera->frame;
      app->camera->focus = app->iocamera->focus;
      reset_display(app);
    }

    // selection
    if ((input.mouse_left || input.mouse_right) && input.modifier_alt &&
        !input.widgets_active) {
      auto ij = image_coords(input.mouse_pos, app->glparams.center,
          app->glparams.scale, app->render.imsize());
      if (ij.x >= 0 && ij.x < app->render.width() && ij.y >= 0 &&
          ij.y < app->render.height()) {
        auto ray = camera_ray(app->camera->frame, app->camera->lens,
            app->camera->lens, app->camera->film,
            vec2f{ij.x + 0.5f, ij.y + 0.5f} /
                vec2f{(float)app->render.width(), (float)app->render.height()});
        if (auto isec = intersect_bvh(app->bvh, ray); isec.hit) {
          app->selected_instance = app->ioscene->instances[isec.instance];
        }
      }
    }
  };

  // run ui
  run_ui({1280 + 320, 720}, "yscnitrace", callbacks);

  // done
  return 0;
}
