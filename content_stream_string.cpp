#include <cmath>
#include <format>
#include <stdexcept>
#include <string>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "content_stream_string.h"

ContentStreamString::ContentStreamString(bool path_graphics_state):
  std::string(),
  trailer_length(0),
  have_last_coord(false),
  last_coord(0.0, 0.0)
{
  if (path_graphics_state)
  {
    *this += "q Q\n";
    trailer_length = 2;
  }
}

ContentStreamString& ContentStreamString::insert(const std::string s)
{
  std::string::insert(this->length() - trailer_length, s);
  return *this;
}

ContentStreamString& ContentStreamString::set_color_space(const std::string color_space,
							  bool fill,
							  bool stroke)
{
  if (fill)
    this->insert("/" + color_space + "cs ");
  if (stroke)
    this->insert("/" + color_space + "CS ");
  return *this;
}

ContentStreamString& ContentStreamString::set_color(Color color,
						    bool fill,
						    bool stroke)
{
  if (fill)
    this->insert(std::format("{0:g} {1:g} {2:g} sc ", color.r, color.g, color.b));
  if (stroke)
    this->insert(std::format("{0:g} {1:g} {2:g} SC ", color.r, color.g, color.b));
  return *this;
}

ContentStreamString& ContentStreamString::set_line_width(float width)
{
  this->insert(std::format("{0:g} w ", width));
  return *this;
}

ContentStreamString& ContentStreamString::move_to(Coord dest)
{
  this->insert(std::format("{0:g} {1:g} m ", dest.x, dest.y));
  last_coord = dest;
  have_last_coord = true;
  return *this;
}

ContentStreamString& ContentStreamString::line_to(Coord dest)
{
  this->insert(std::format("{0:g} {1:g} l ", dest.x, dest.y));
  last_coord = dest;
  have_last_coord = true;
  return *this;
}

ContentStreamString& ContentStreamString::arc_to(Coord dest,
						 bool clockwise)
{
  if (! have_last_coord)
    throw std::logic_error("arc_to() origin unknown");

  Coord p0 = last_coord;
  Coord p1 = p0;
  Coord p2 = dest;
  Coord p3 = dest;

  double radius = std::abs(p0.x - p3.x);  // WARNING: must be same as abs(p0.y - p3.y)
  double c = radius * 4 * (std::sqrt(2.0) - 1.0) / 3;

  if (clockwise)
  {
    if ((p0.x < p3.x) && (p0.y > p3.y))
    {
      // first quadrant
      p1.x += c;
      p2.y += c;
    }
    else if ((p0.x < p3.x) && (p0.y < p3.y))
    {
      // second quadrant
      p1.y += c;
      p2.x -= c;
    }
    else if ((p0.x > p3.x) && (p0.y < p3.y))
    {
      // third quadrant
      p1.x -= c;
      p2.y -= c;
    }
    else
    {
      // fourth quadrant
      p1.y -= c;
      p2.x += c;
    }
  }
  else
  {
    // counterclockwise - NOT TESTED
    if ((p0.x > p3.x) && (p0.y < p3.y))
    {
      // first quadrant
      p1.y += c;
      p2.x += c;
    }
    else if ((p0.x > p3.y) && (p0.y > p3.y))
    {
      // second quadrant
      p1.x -= c;
      p2.y += c;
    }
    else if ((p0.x < p3.y) && (p0.y > p3.y))
    {
      // third quadrant
      p1.y -= c;
      p2.x -= c;
    }
    else
    {
      // fourth quadrant
      p1.x += c;
      p2.y -= c;
    }
  }

  insert(std::format("{0:g} {1:g} {2:g} {3:g} {4:g} {5:g} c\n",
		     p1.x, p1.y, p2.x, p2.y, p3.x, p3.y));

  last_coord = dest;
  have_last_coord = true;
  return *this;
}

ContentStreamString& ContentStreamString::rect(Dimensions dimensions)
{
  if (! have_last_coord)
    throw std::logic_error("arc_to() origin unknown");

  Coord origin = last_coord;	// top left, if width and height are both positive

										// side, if origin is top left
  line_to({ origin.x + dimensions.width, origin.y });				// top
  line_to({ origin.x + dimensions.width, origin.y - dimensions.height });	// right
  line_to({ origin.x,                    origin.y - dimensions.height });	// bottom
  line_to(origin);								// left

  return *this;
}

ContentStreamString& ContentStreamString::rounded_rect(Dimensions dimensions,
						       double radius)
{
  if (! have_last_coord)
    throw std::logic_error("arc_to() origin unknown");

  if (radius == 0.0)
    return rect(dimensions);

  Coord origin = last_coord;	// top left, if width and height are both positive

  move_to({ origin.x,                             origin.y - radius });				// top end of left side segment
  arc_to ({ origin.x + radius,                    origin.y });                 			// top left corner
  line_to({ origin.x + dimensions.width - radius, origin.y });					// top segment
  arc_to ({ origin.x + dimensions.width,          origin.y - radius });				// top right corner
  line_to({ origin.x + dimensions.width,          origin.y - dimensions.height + radius });	// right segment
  arc_to ({ origin.x + dimensions.width - radius, origin.y - dimensions.height });		// bottom right corner
  line_to({ origin.x + radius,                    origin.y - dimensions.height });		// bottom segment
  arc_to ({ origin.x,                             origin.y - dimensions.height + radius });	// bottom left corner
  line_to({ origin.x,                             origin.y - radius });				// left segment
  move_to(origin);

  return *this;
}


ContentStreamString& ContentStreamString::text(Coord dest,
					       HorizontalAlignment horizontal_alignment,
					       const std::string& text,
					       const std::string& font_name,
					       double font_size_pt)
{
  double width = 0.0;
  double x;

  switch (horizontal_alignment)
  {
  case HorizontalAlignment::LEFT:
    x = dest.x;
    break;
  case HorizontalAlignment::CENTER:
    x = dest.x - width / 2.0;
    break;
  case HorizontalAlignment::RIGHT:
    x = dest.x - width;
    break;
  }

  insert("BT ");							// begin text object
  insert(std::format("{0:g} {1:g} Td ", dest.x, dest.y));		// text position
  insert("0 Tr ");							// text render mode fill
  insert("/" + font_name + std::format(" {0:g} Tf\n", font_size_pt));	// select font and size
  insert("(" + text + ") Tj ");
  insert("ET\n");							// end text object

  return *this;
}


ContentStreamString& ContentStreamString::path_close()
{
  this->insert("h\n");  // close
  have_last_coord = false;
  return *this;
}

ContentStreamString& ContentStreamString::path_stroke()
{
  this->insert("S\n");  // stroke
  return *this;
}

ContentStreamString& ContentStreamString::path_close_stroke()
{
  this->insert("s\n");  // close, stroke
  have_last_coord = false;
  return *this;
}

ContentStreamString& ContentStreamString::path_fill(FillRule fill_rule)
{
  if (fill_rule == FillRule::NONZERO_WINDING)
    this->insert("f\n");  // fill
  else
    this->insert("f*\n");  // fill
  return *this;
}

ContentStreamString& ContentStreamString::path_fill_stroke(FillRule fill_rule)
{
  if (fill_rule == FillRule::NONZERO_WINDING)
    this->insert("B\n");  // fill and stroke
  else
    this->insert("B*\n");  // fill and stroke
  return *this;
}

ContentStreamString& ContentStreamString::path_close_fill_stroke(FillRule fill_rule)
{
  if (fill_rule == FillRule::NONZERO_WINDING)
    this->insert("b\n");  // close, fill and stroke
  else
    this->insert("b*\n");  // close, fill and stroke
  have_last_coord = false;
  return *this;
}
