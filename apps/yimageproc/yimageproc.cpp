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

#include <yocto/yocto_commonio.h>
#include <yocto/yocto_image.h>
#include <yocto/yocto_math.h>
using namespace yocto;

namespace yocto {

image<vec4f> filter_bilateral(const image<vec4f>& img, float spatial_sigma,
    float range_sigma, const vector<image<vec4f>>& features,
    const vector<float>& features_sigma) {
  auto filtered     = image{img.imsize(), zero4f};
  auto filter_width = (int)ceil(2.57f * spatial_sigma);
  auto sw           = 1 / (2.0f * spatial_sigma * spatial_sigma);
  auto rw           = 1 / (2.0f * range_sigma * range_sigma);
  auto fw           = vector<float>();
  for (auto feature_sigma : features_sigma)
    fw.push_back(1 / (2.0f * feature_sigma * feature_sigma));
  for (auto j = 0; j < img.height(); j++) {
    for (auto i = 0; i < img.width(); i++) {
      auto av = zero4f;
      auto aw = 0.0f;
      for (auto fj = -filter_width; fj <= filter_width; fj++) {
        for (auto fi = -filter_width; fi <= filter_width; fi++) {
          auto ii = i + fi, jj = j + fj;
          if (ii < 0 || jj < 0) continue;
          if (ii >= img.width() || jj >= img.height()) continue;
          auto uv  = vec2f{float(i - ii), float(j - jj)};
          auto rgb = img[{i, j}] - img[{i, j}];
          auto w   = (float)exp(-dot(uv, uv) * sw) *
                   (float)exp(-dot(rgb, rgb) * rw);
          for (auto fi = 0; fi < features.size(); fi++) {
            auto feat = features[fi][{i, j}] - features[fi][{i, j}];
            w *= exp(-dot(feat, feat) * fw[fi]);
          }
          av += w * img[{ii, jj}];
          aw += w;
        }
      }
      filtered[{i, j}] = av / aw;
    }
  }
  return filtered;
}

image<vec4f> filter_bilateral(
    const image<vec4f>& img, float spatial_sigma, float range_sigma) {
  auto filtered = image{img.imsize(), zero4f};
  auto fwidth   = (int)ceil(2.57f * spatial_sigma);
  auto sw       = 1 / (2.0f * spatial_sigma * spatial_sigma);
  auto rw       = 1 / (2.0f * range_sigma * range_sigma);
  for (auto j = 0; j < img.height(); j++) {
    for (auto i = 0; i < img.width(); i++) {
      auto av = zero4f;
      auto aw = 0.0f;
      for (auto fj = -fwidth; fj <= fwidth; fj++) {
        for (auto fi = -fwidth; fi <= fwidth; fi++) {
          auto ii = i + fi, jj = j + fj;
          if (ii < 0 || jj < 0) continue;
          if (ii >= img.width() || jj >= img.height()) continue;
          auto uv  = vec2f{float(i - ii), float(j - jj)};
          auto rgb = img[{i, j}] - img[{ii, jj}];
          auto w   = exp(-dot(uv, uv) * sw) * exp(-dot(rgb, rgb) * rw);
          av += w * img[{ii, jj}];
          aw += w;
        }
      }
      filtered[{i, j}] = av / aw;
    }
  }
  return filtered;
}

bool make_image_preset(const string& type, image<vec4f>& img, string& error) {
  auto set_region = [](image<vec4f>& img, const image<vec4f>& region,
                        const vec2i& offset) {
    for (auto j = 0; j < region.height(); j++) {
      for (auto i = 0; i < region.width(); i++) {
        if (!img.contains({i, j})) continue;
        img[vec2i{i, j} + offset] = region[{i, j}];
      }
    }
  };

  auto size = vec2i{1024, 1024};
  if (type.find("sky") != type.npos) size = {2048, 1024};
  if (type.find("images2") != type.npos) size = {2048, 1024};
  if (type == "grid") {
    img = make_grid(size);
  } else if (type == "checker") {
    img = make_checker(size);
  } else if (type == "bumps") {
    img = make_bumps(size);
  } else if (type == "uvramp") {
    img = make_uvramp(size);
  } else if (type == "gammaramp") {
    img = make_gammaramp(size);
  } else if (type == "blackbodyramp") {
    img = make_blackbodyramp(size);
  } else if (type == "uvgrid") {
    img = make_uvgrid(size);
  } else if (type == "colormap") {
    img = make_colormapramp(size);
    img = srgb_to_rgb(img);
  } else if (type == "sky") {
    img = make_sunsky(
        size, pif / 4, 3.0, false, 1.0, 1.0, vec3f{0.7, 0.7, 0.7});
  } else if (type == "sunsky") {
    img = make_sunsky(size, pif / 4, 3.0, true, 1.0, 1.0, vec3f{0.7, 0.7, 0.7});
  } else if (type == "noise") {
    img = make_noisemap(size, 1);
  } else if (type == "fbm") {
    img = make_fbmmap(size, 1);
  } else if (type == "ridge") {
    img = make_ridgemap(size, 1);
  } else if (type == "turbulence") {
    img = make_turbulencemap(size, 1);
  } else if (type == "bump-normal") {
    img = make_bumps(size);
    img = srgb_to_rgb(bump_to_normal(img, 0.05f));
  } else if (type == "images1") {
    auto sub_types = vector<string>{"grid", "uvgrid", "checker", "gammaramp",
        "bumps", "bump-normal", "noise", "fbm", "blackbodyramp"};
    auto sub_imgs  = vector<image<vec4f>>(sub_types.size());
    for (auto i = 0; i < sub_imgs.size(); i++) {
      if (!make_image_preset(sub_types[i], sub_imgs[i], error)) return false;
    }
    auto montage_size = zero2i;
    for (auto& sub_img : sub_imgs) {
      montage_size.x += sub_img.width();
      montage_size.y = max(montage_size.y, sub_img.height());
    }
    img      = image<vec4f>(montage_size);
    auto pos = 0;
    for (auto& sub_img : sub_imgs) {
      set_region(img, sub_img, {pos, 0});
      pos += sub_img.width();
    }
  } else if (type == "images2") {
    auto sub_types = vector<string>{"sky", "sunsky"};
    auto sub_imgs  = vector<image<vec4f>>(sub_types.size());
    for (auto i = 0; i < sub_imgs.size(); i++) {
      if (!make_image_preset(sub_types[i], sub_imgs[i], error)) return false;
    }
    auto montage_size = zero2i;
    for (auto& sub_img : sub_imgs) {
      montage_size.x += sub_img.width();
      montage_size.y = max(montage_size.y, sub_img.height());
    }
    img      = image<vec4f>(montage_size);
    auto pos = 0;
    for (auto& sub_img : sub_imgs) {
      set_region(img, sub_img, {pos, 0});
      pos += sub_img.width();
    }
  } else if (type == "test-floor") {
    img = make_grid(size);
    img = add_border(img, 0.0025);
  } else if (type == "test-grid") {
    img = make_grid(size);
  } else if (type == "test-checker") {
    img = make_checker(size);
  } else if (type == "test-bumps") {
    img = make_bumps(size);
  } else if (type == "test-uvramp") {
    img = make_uvramp(size);
  } else if (type == "test-gammaramp") {
    img = make_gammaramp(size);
  } else if (type == "test-blackbodyramp") {
    img = make_blackbodyramp(size);
  } else if (type == "test-colormapramp") {
    img = make_colormapramp(size);
    img = srgb_to_rgb(img);
  } else if (type == "test-uvgrid") {
    img = make_uvgrid(size);
  } else if (type == "test-sky") {
    img = make_sunsky(
        size, pif / 4, 3.0, false, 1.0, 1.0, vec3f{0.7, 0.7, 0.7});
  } else if (type == "test-sunsky") {
    img = make_sunsky(size, pif / 4, 3.0, true, 1.0, 1.0, vec3f{0.7, 0.7, 0.7});
  } else if (type == "test-noise") {
    img = make_noisemap(size);
  } else if (type == "test-fbm") {
    img = make_noisemap(size);
  } else if (type == "test-bumps-normal") {
    img = make_bumps(size);
    img = bump_to_normal(img, 0.05);
  } else if (type == "test-bumps-displacement") {
    img = make_bumps(size);
    img = srgb_to_rgb(img);
  } else if (type == "test-fbm-displacement") {
    img = make_fbmmap(size);
    img = srgb_to_rgb(img);
  } else if (type == "test-checker-opacity") {
    img = make_checker(size, 1, {1, 1, 1, 1}, {0, 0, 0, 0});
  } else if (type == "test-grid-opacity") {
    img = make_grid(size, 1, {1, 1, 1, 1}, {0, 0, 0, 0});
  } else {
    error = "unknown preset";
    img   = {};
    return false;
  }
  return true;
}

}  // namespace yocto

int main(int argc, const char* argv[]) {
  // command line parameters
  auto tonemap_on          = false;
  auto tonemap_exposure    = 0;
  auto tonemap_filmic      = false;
  auto logo                = false;
  auto resize_width        = 0;
  auto resize_height       = 0;
  auto spatial_sigma       = 0.0f;
  auto range_sigma         = 0.0f;
  auto alpha_to_color      = false;
  auto alpha_filename      = ""s;
  auto coloralpha_filename = ""s;
  auto diff_filename       = ""s;
  auto diff_signal         = false;
  auto diff_threshold      = 0.0f;
  auto output              = "out.png"s;
  auto filename            = "img.hdr"s;

  // parse command line
  auto cli = make_cli("yimgproc", "Transform images");
  add_option(cli, "--tonemap/--no-tonemap", tonemap_on, "Tonemap image");
  add_option(cli, "--exposure,-e", tonemap_exposure, "Tonemap exposure");
  add_option(
      cli, "--filmic/--no-filmic", tonemap_filmic, "Tonemap uses filmic curve");
  add_option(cli, "--resize-width", resize_width,
      "resize size (0 to maintain aspect)");
  add_option(cli, "--resize-height", resize_height,
      "resize size (0 to maintain aspect)");
  add_option(cli, "--spatial-sigma", spatial_sigma, "blur spatial sigma");
  add_option(cli, "--range-sigma", range_sigma, "bilateral blur range sigma");
  add_option(
      cli, "--set-alpha", alpha_filename, "set alpha as this image alpha");
  add_option(cli, "--set-color-as-alpha", coloralpha_filename,
      "set alpha as this image color");
  add_option(cli, "--alpha-to-color/--no-alpha-to-color", alpha_to_color,
      "Set color as alpha");
  add_option(cli, "--logo/--no-logo", logo, "Add logo");
  add_option(cli, "--diff", diff_filename, "compute the diff between images");
  add_option(cli, "--diff-signal", diff_signal, "signal a diff as error");
  add_option(cli, "--diff-threshold,", diff_threshold, "diff threshold");
  add_option(cli, "--output,-o", output, "output image filename");
  add_option(cli, "filename", filename, "input image filename", true);
  parse_cli(cli, argc, argv);

  // error string buffer
  auto error = ""s;

  // load
  auto ioerror = ""s;
  auto img     = image<vec4f>{};
  if (path_extension(filename) == ".ypreset") {
    if (!make_image_preset(path_basename(filename), img, ioerror))
      print_fatal(ioerror);
  } else {
    if (!load_image(filename, img, ioerror)) print_fatal(ioerror);
  }

  // set alpha
  if (alpha_filename != "") {
    auto alpha = image<vec4f>{};
    if (!load_image(alpha_filename, alpha, ioerror)) print_fatal(ioerror);
    if (img.imsize() != alpha.imsize()) print_fatal("bad image size");
    for (auto j = 0; j < img.height(); j++)
      for (auto i = 0; i < img.width(); i++) img[{i, j}].w = alpha[{i, j}].w;
  }

  // set alpha
  if (coloralpha_filename != "") {
    auto alpha = image<vec4f>{};
    if (!load_image(coloralpha_filename, alpha, ioerror)) print_fatal(ioerror);
    if (img.imsize() != alpha.imsize()) print_fatal("bad image size");
    for (auto j = 0; j < img.height(); j++)
      for (auto i = 0; i < img.width(); i++)
        img[{i, j}].w = mean(xyz(alpha[{i, j}]));
  }

  // set color from alpha
  if (alpha_to_color) {
    for (auto& c : img) c = vec4f{c.w, c.w, c.w, c.w};
  }

  // diff
  if (diff_filename != "") {
    auto diff = image<vec4f>{};
    if (!load_image(diff_filename, diff, ioerror)) print_fatal(ioerror);
    if (img.imsize() != diff.imsize()) print_fatal("image sizes are different");
    img = image_difference(img, diff, true);
  }

  // resize
  if (resize_width != 0 || resize_height != 0) {
    img = resize_image(img, {resize_width, resize_height});
  }

  // bilateral
  if (spatial_sigma != 0 && range_sigma != 0) {
    img = filter_bilateral(img, spatial_sigma, range_sigma, {}, {});
  }

  // hdr correction
  if (tonemap_on) {
    img = tonemap_image(img, tonemap_exposure, tonemap_filmic, false);
  }

  // save
  if (!save_image(output, logo ? add_logo(img) : img, ioerror))
    print_fatal(ioerror);

  // check diff
  if (diff_filename != "" && diff_signal) {
    for (auto& c : img) {
      if (max(xyz(c)) > diff_threshold) print_fatal("image content differs");
    }
  }

  // done
  return 0;
}
