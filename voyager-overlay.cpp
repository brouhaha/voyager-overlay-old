// Generate Voyager calculator keyboard overlays
// Copyright 2023 Eric Smith <spacewar@gmail.com>
// SPDX-License-Identifier: GPL-3.0-only

#include <format>
#include <iostream>
#include <stdexcept>
#include <vector>

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>
#include <qpdf/QPDFWriter.hh>
#include <qpdf/QUtil.hh>

#include "content_stream_string.h"


static constexpr double mm_per_in = 25.4;
static constexpr double pt_per_in = 72.0;


static constexpr double PAGE_INSET_LEFT_IN   = 0.625;
static constexpr double PAGE_INSET_RIGHT_IN  = 0.625;
static constexpr double PAGE_INSET_TOP_IN    = 0.625;
static constexpr double PAGE_INSET_BOTTOM_IN = 1.024;


struct RegistrationGeometry
{
  double inset_left_in;
  double inset_right_in;
  double inset_top_in;
  double inset_bottom_in;

  double square_size_in;
  double line_length_in;
  double line_width_in;
};


struct OverlayGeometry
{
  double width_in;
  double height_in;
  double corner_radius_in;

  double key_col_pitch_in;
  double key_row_pitch_in;
  double key_row_1_offset_in;

  double key_width_in;
  double key_height_in;
  double key_corner_radius_in;
};


static std::string create_registration(double page_width_in,
				       double page_height_in,
				       const RegistrationGeometry& geom)
{
  ContentStreamString s(true);

  s.set_line_width(geom.line_width_in);
  s.set_color_space("DeviceRGB", true, true);
  s.set_color(BLACK, true, true);

  // square at top left of cut area
  s.move_to({ geom.inset_left_in,                                        page_height_in - geom.inset_top_in});	// top left
  s.rect({ geom.square_size_in, geom.square_size_in });
  s.path_close_fill_stroke();
  
  // right angle at bottom left of cut area
  s.move_to({ geom.inset_left_in,                                        geom.inset_bottom_in + geom.line_length_in });
  s.line_to({ geom.inset_left_in,                                        geom.inset_bottom_in });
  s.line_to({ geom.inset_left_in + geom.line_length_in,                  geom.inset_bottom_in });
  s.path_stroke();

  // right angle at top left of cut area
  s.move_to({ page_width_in - geom.inset_right_in - geom.line_length_in, page_height_in - geom.inset_top_in });
  s.line_to({ page_width_in - geom.inset_right_in,                       page_height_in - geom.inset_top_in });
  s.line_to({ page_width_in - geom.inset_right_in,                       page_height_in - geom.inset_top_in - geom.line_length_in});
  s.path_stroke();

  return s;
}


static std::string create_overlay(const OverlayGeometry& geom)
{
  std::string s;
  double line_width_mm = 0.1;
  double line_width_in = line_width_mm / mm_per_in;

  ContentStreamString cs(true);
  cs.set_line_width(line_width_mm / mm_per_in);
  cs.set_color(BLACK, false, true);	// set stroke color

  cs.move_to({ 0.0, geom.height_in });
  cs.rounded_rect({ geom.width_in, geom.height_in}, geom.corner_radius_in);
  cs.path_close_stroke();

  for (int row = 0; row < 4; row++)
  {
    double y = geom.height_in - row * geom.key_row_pitch_in - geom.key_row_1_offset_in;
    for (int col = 0; col < 10; col++)
    {
      if ((row == 3) && (col == 5))
	continue;  // ignore bottom half of enter key
      double key_height = geom.key_height_in;
      if ((row == 2) && (col == 5))
	key_height += geom.key_row_pitch_in;	// if top half of enter key, it's a tall key
      double x = geom.width_in / 2.0 - (5 * geom.key_col_pitch_in) + (geom.key_col_pitch_in - geom.key_width_in) / 2.0 + col * geom.key_col_pitch_in;
      cs.move_to({ x, y });
      cs.rounded_rect({ geom.key_width_in, key_height }, geom.key_corner_radius_in);
      cs.path_close_stroke();

      cs.text({ x + geom.key_width_in / 2.0, y + 0.03 },
	      HorizontalAlignment::CENTER,
	      "Hello",
	      "F1",
	      6.0 / pt_per_in);
    }
  }

  return cs;
}


constexpr RegistrationGeometry cameo4_no_mat_reg_geometry =
{
  .inset_left_in   = 0.625,
  .inset_right_in  = 0.625,
  .inset_top_in    = 0.625,
  .inset_bottom_in = 1.024,

  .square_size_in  = 0.2,
  .line_length_in  = 0.5,
  .line_width_in   = 0.5 / mm_per_in
};


static constexpr float OVERLAY_MINIMUM_Y_GAP_IN = 0.1;


static QPDFObjectHandle createPageContents(QPDF& pdf,
					   double page_width_in,
					   double page_height_in,
					   const OverlayGeometry& geom,
					   bool show_outlines,
					   bool show_reg_marks)
{
  // Create a stream that displays our image and the given text in
  // our font.
  std::string contents;

  static double available_height_in = page_height_in - PAGE_INSET_TOP_IN - PAGE_INSET_BOTTOM_IN;
  static int y_count = available_height_in / geom.height_in;
  if ((available_height_in - ((y_count * geom.height_in) / (y_count - 1))) < OVERLAY_MINIMUM_Y_GAP_IN)
    y_count--;
  static double overlay_y_gap_in = (available_height_in - (y_count * geom.height_in)) / (y_count - 1);

  contents = ("q "                                // push graphics stack
	      + std::format("{0:g} 0 0 {0:g} 0 0 cm ", pt_per_in)	// transform to inch coordinate system, origin at top left
	      );

  if (show_reg_marks)
    contents += create_registration(page_width_in, page_height_in, cameo4_no_mat_reg_geometry);

  //for (int y = 0; y < y_count; y++)
  for (int y = 1; y < 2; y++)
  {
    double left = (page_width_in - geom.width_in) / 2.0;
    double bottom = page_height_in - PAGE_INSET_TOP_IN - ((y + 1) * geom.height_in) - (y * overlay_y_gap_in);
    contents += ("q "
		 + std::format("1 0 0 1 {0:g} {1:g} cm\n", left, bottom));

    contents += create_overlay(geom);

    contents += "Q\n";
  }

  contents += "Q\n";

  return pdf.newStream(contents);
  //return QPDFObjectHandle::newStream(&pdf, contents);
}


constexpr double letter_width_in = 8.5;
constexpr double letter_height_in = 11.0;

constexpr double letter_width_pt = letter_width_in * pt_per_in;
constexpr double letter_height_pt = letter_height_in * pt_per_in;


static void create_page(QPDFPageDocumentHelper &dh,
			std::string font_name,
			QPDFObjectHandle font_obj,
			const OverlayGeometry& geom,
			bool do_outlines,
			bool do_reg_marks)
{
  QPDF& pdf(dh.getQPDF());
  
  // Create direct objects as needed by the page dictionary.
  QPDFObjectHandle procset = "[/PDF /Text]"_qpdf;

  QPDFObjectHandle rfont = QPDFObjectHandle::newDictionary();
  rfont.replaceKey("/" + font_name, font_obj);

  QPDFObjectHandle resources = QPDFObjectHandle::newDictionary();
  resources.replaceKey("/ProcSet", procset);
  resources.replaceKey("/Font", rfont);

  // Create the page content stream
  QPDFObjectHandle contents =
    createPageContents(pdf,
		       letter_width_in,
		       letter_height_in,
		       geom,
		       do_outlines,  // show_outlines
		       do_reg_marks);// show_reg_marks

  // Create the page dictionary
  std::string page_dict_stream_str = ("<<"
				      " /Type /Page"
				      " /MediaBox [0 0 "
				      + std::format("{0:g} {1:g}", letter_width_pt, letter_height_pt) +
				      "]"
				      ">>");

  QPDFObjectHandle page = pdf.makeIndirectObject(QPDFObjectHandle::parse(page_dict_stream_str));

  page.replaceKey("/Contents", contents);
  page.replaceKey("/Resources", resources);

  // Add the page to the PDF file
  dh.addPage(page, false);
}


static void create_pdf(const std::string filename,
		       const OverlayGeometry& geom,
		       bool do_outlines,
		       bool do_reg_marks)
{
  QPDF pdf;

  pdf.emptyPDF();

  QPDFObjectHandle font_obj = pdf.makeIndirectObject(
        // line-break
        "<<"
        " /Type /Font"
        " /Subtype /Type1"
        " /Name /F1"
        " /BaseFont /Helvetica"
        " /Encoding /WinAnsiEncoding"
        ">>"_qpdf);

  QPDFPageDocumentHelper dh(pdf);

  create_page(dh,
	      "F1",        // font_name
	      font_obj,    // font_obj
	      geom,
	      do_outlines,
	      do_reg_marks);

  QPDFWriter w(pdf, filename.c_str());
  w.write();
}


void conflicting_options(const po::variables_map& vm,
			 std::initializer_list<std::string> list,
			 bool required = false)
{
  bool found = false;
  for (std::initializer_list<std::string>::iterator it1 = list.begin();
       it1 != list.end();
       ++it1)
  {
    if (vm.count(*it1) &&
	! vm[*it1].defaulted())
      found = true;
    else
      continue;
    for (std::initializer_list<std::string>::iterator it2 = it1;
	 ++it2 != list.end();
	 )
    {
      if (vm.count(*it1) &&
	  !vm[*it1].defaulted() &&
	  vm.count(*it2) &&
	  !vm[*it2].defaulted())
	throw std::logic_error("Conflicting options `" + *it1 + "' and '" + *it2 + "'.");
    }
  }
  if (required && ! found)
    throw std::logic_error("No option in group set");
}


constexpr OverlayGeometry hp_geometry =
{
  .width_in             = 4.65,
  .height_in            = 2.10,
  .corner_radius_in     = 0.025,

  .key_col_pitch_in     = 0.45,
  .key_row_pitch_in     = 0.50,
  .key_row_1_offset_in  = 0.133,

  .key_width_in         = 0.34,
  .key_height_in        = 0.32,
  .key_corner_radius_in = 0.025
};

constexpr OverlayGeometry sm_geometry =
{
  .width_in             = 4.75,
  .height_in            = 1.95,
  .corner_radius_in     = 0.025,

  .key_col_pitch_in     = 0.475,
  .key_row_pitch_in     = 0.475,
  .key_row_1_offset_in  = 0.175,

  .key_width_in         = 0.33,
  .key_height_in        = 0.30,
  .key_corner_radius_in = 0.025
};


int main(int argc, char* argv[])
{
  std::string type;
  std::string model;
  bool do_reg_marks = false;
  bool do_outlines = false; 
  bool do_legends = false;
  const OverlayGeometry* geom = nullptr;

  try
  {
    po::options_description desc("Options");
    desc.add_options()
      ("help,h",   "output help message")
      ("cut,c",    "cut marks")
      ("print,p",  "print (registration and legends)")
      ("all,a",    "all (registration, legends, and cut marks)")
      ("hp",       "HP calculator")
      ("sm",       "Swiss Micros calculator")
      ("output,o", po::value<std::string>(), "output PDF file")
      ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);

    po::notify(vm);

    if (vm.count("help"))
    {
      std::cout << desc << "\n";
      return 0;
    }

    conflicting_options(vm, {"cut", "print", "all"}, true);
    conflicting_options(vm, {"hp", "sm"});

    if (vm.count("cut"))
    {
      type = "cut";
      do_outlines  = true;
    }
    if (vm.count("print"))
    {
      type = "print";
      do_reg_marks = true;
      do_legends = true;
    }
    if (vm.count("all"))
    {
      type = "all";
      do_reg_marks = true;
      do_legends = true;
      do_outlines  = false;
    }

    if (vm.count("sm"))
    {
      model = "dm1xl";
      geom = & sm_geometry;
    }
    else
    {
      model = "voyager";
      geom = & hp_geometry;
    }
  }
  catch (std::exception& e)
  {
    std::cerr << "error: " << e.what() << "\n";
    return 1;
  }

  std::string filename = model + "-overlay-" + type + ".pdf";
  create_pdf(filename,
	     *geom,
	     do_outlines,
	     do_reg_marks);

  return 0;
}
