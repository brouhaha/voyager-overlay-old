// Copyright 2023 Eric Smith
// SPDX-License-Identifier: GPL-3.0-only

#ifndef CONTENT_STREAM_STRING_H
#define CONTENT_STREAM_STRING_H

struct Coord { double x; double y; };

struct Dimensions { double width; double height; };

struct Color { double r; double g; double b; };

constexpr Color BLACK { 0.0, 0.0, 0.0 };
constexpr Color WHITE { 1.0, 1.0, 1.0 };

enum struct FillRule
{
  NONZERO_WINDING,
  EVEN_ODD
};

enum struct HorizontalAlignment
{
  LEFT,
  CENTER,
  RIGHT
};

class ContentStreamString: public std::string
{
public:
  ContentStreamString(bool push_graphics_state);

  ContentStreamString& set_color_space(const std::string color_space,
				       bool fill,
				       bool stroke);

  ContentStreamString& set_color(Color color,
				 bool fill,
				 bool stroke);

  ContentStreamString& set_line_width(float width);

  ContentStreamString& move_to(Coord dest);
  ContentStreamString& line_to(Coord dest);

  // WARNING: ONLY a 90 degree arc with orientation of a multiple of 90 degrees can be generated
  ContentStreamString& arc_to(Coord dest,
			      bool clockwise = true);
  
  ContentStreamString& rect(Dimensions dimensions);
  ContentStreamString& rounded_rect(Dimensions dimensions,
				    double radius);

  ContentStreamString& text(Coord dest,
			    HorizontalAlignment horizontal_alignment,
			    const std::string& text,
			    const std::string& font_name,
			    double font_size_pt);

  ContentStreamString& path_close();
  ContentStreamString& path_stroke();
  ContentStreamString& path_close_stroke();
  ContentStreamString& path_fill(FillRule fill_rule = FillRule::NONZERO_WINDING);
  ContentStreamString& path_fill_stroke(FillRule fill_rule = FillRule::NONZERO_WINDING);
  ContentStreamString& path_close_fill_stroke(FillRule fill_rule = FillRule::NONZERO_WINDING);
  

private:
  size_type trailer_length;
  bool have_last_coord;
  Coord last_coord;

  ContentStreamString& insert(const std::string s);
};

#endif // CONTENT_STREAM_STRING_H

