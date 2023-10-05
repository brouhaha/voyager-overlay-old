// Generate Voyager calculator keyboard overlays
// Copyright 2023 Eric Smith
// SPDX-License-Identifier: GPL-3.0-only

#include <format>
#include <iostream>
#include <map>
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


static constexpr double MM_PER_IN = 25.4;
static constexpr double PT_PER_IN = 72.0;


static constexpr double PAGE_INSET_LEFT_IN     = 0.625;
static constexpr double PAGE_INSET_RIGHT_IN    = 0.625;
static constexpr double PAGE_INSET_TOP_IN      = 0.625;
static constexpr double PAGE_INSET_BOTTOM_IN   = 1.024;

static constexpr double REG_MARK_SIZE_IN       = 0.250;
static constexpr double REG_MARK_LINE_WIDTH_MM = 0.5;


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


static std::map<int, std::string> legend_map =
{
#if 1
  { 11, "ln e^x"   },
  { 12, "log 10^x" },
  { 13, "? fact"   },
  { 14, "sin -1"   },
  { 15, "cos -1"   },
  { 16, "tan -1"   },
#else
  { 11, "SL"       },
  { 12, "SR"       },
  { 13, "RL"       },
  { 14, "RR"       },
  { 15, "RLn"      },
  { 16, "RRn"      },
#endif
  { 17, "MASKL"    },
  { 18, "MASKR"    },
  { 19, "RMD"      },
  { 10, "XOR"      },

  { 21, "x<>(i)"   },
  { 22, "x<>I"     },
  { 23, "SH HEX"   },
  { 24, "SH DEC"   },
  { 25, "SH OCT"   },
  { 26, "SH BIN"   },
  { 27, "SB"       },
  { 28, "CB"       },
  { 29, "B?"       },
  { 30, "AND"      },

  { 31, "(i)"      },
  { 32, "I"        },
  { 33, "CL PRGM"  },
  { 34, "CL REG"   },
  { 35, "CL PRFX"  },
  { 36, "WINDOW"   },
  { 37, "1s COMP"  },
  { 38, "2s COMP"  },
  { 39, "UNSIGNED" },
  { 40, "NOT"      },

  { 41, ""         },
  { 42, ""         },
  { 43, ""         },
  { 44, "WSIZE"    },
  { 45, "FLOAT"    },

  { 47, "MEM"      },
  { 48, "STATUS"   },
  { 49, "EEX"      },
  { 40, "OR"       },
};


static std::string create_overlay(const OverlayGeometry& geom,
				  bool show_outlines,
				  bool show_legends)
{
  std::string s;
  double line_width_mm = 0.1;
  double line_width_in = line_width_mm / MM_PER_IN;

  ContentStreamString cs(true);
  cs.set_line_width(line_width_mm / MM_PER_IN);
  cs.set_color(BLACK, false, true);	// set stroke color

  if (show_outlines)
  {
    cs.move_to({ 0.0, geom.height_in });
    cs.rounded_rect({ geom.width_in, geom.height_in}, geom.corner_radius_in);
    cs.path_close_stroke();
  }

  for (int row = 0; row < 4; row++)
  {
    double y = geom.height_in - (row * geom.key_row_pitch_in + geom.key_row_1_offset_in);
    for (int col = 0; col < 10; col++)
    {
      if ((row == 3) && (col == 5))
	continue;  // ignore bottom half of enter key
      double key_height = geom.key_height_in;
      if ((row == 2) && (col == 5))
	key_height += geom.key_row_pitch_in;	// if top half of enter key, it's a tall key
      int user_kc = (row + 1) * 10 + (col + 1) % 10;

      double x = geom.width_in / 2.0 - (5 * geom.key_col_pitch_in) + (geom.key_col_pitch_in - geom.key_width_in) / 2.0 + col * geom.key_col_pitch_in;

      if (show_outlines)
      {
	cs.move_to({ x, y });
	cs.rounded_rect({ geom.key_width_in, key_height }, geom.key_corner_radius_in);
	cs.path_close_stroke();
      }

      if (show_legends)
      {
	cs.text({ x + geom.key_width_in / 2.0 - 0.125, y + 0.03 },
		HorizontalAlignment::CENTER,
		legend_map[user_kc],
		"F1",
		6.0 / PT_PER_IN);
      }
    }
  }

  return cs;
}


constexpr RegistrationGeometry cameo4_no_mat_reg_geometry =
{
  .inset_left_in   = PAGE_INSET_LEFT_IN,
  .inset_right_in  = PAGE_INSET_RIGHT_IN,
  .inset_top_in    = PAGE_INSET_TOP_IN,
  .inset_bottom_in = PAGE_INSET_BOTTOM_IN,

  .square_size_in  = REG_MARK_SIZE_IN,
  .line_length_in  = REG_MARK_SIZE_IN,
  .line_width_in   = REG_MARK_LINE_WIDTH_MM / MM_PER_IN,
};


static constexpr float OVERLAY_MINIMUM_Y_GAP_IN = 0.1;

static constexpr float ADDITIONAL_INSET_IN = 0.1;

static QPDFObjectHandle createPageContents(QPDF& pdf,
					   double page_width_in,
					   double page_height_in,
					   const RegistrationGeometry& reg_geom,
					   const OverlayGeometry& geom,
					   bool show_outlines,
					   bool show_reg_marks,
					   bool show_legends)
{
  // Create a stream that displays our image and the given text in
  // our font.
  std::string contents;

  static double top_in = reg_geom.inset_top_in + ADDITIONAL_INSET_IN;
  std::cout << "top_in " << top_in << "\n";
  static double bottom_in = page_height_in - (reg_geom.inset_bottom_in + ADDITIONAL_INSET_IN);
  std::cout << "bottom_in " << bottom_in << "\n";
  static double available_height_in = bottom_in - top_in;
  std::cout << "available_height_in_in " << available_height_in << "\n";
  static int y_count = available_height_in / geom.height_in;
  if ((available_height_in - ((y_count * geom.height_in) / (y_count - 1))) < OVERLAY_MINIMUM_Y_GAP_IN)
    y_count--;
  static double overlay_y_gap_in = (available_height_in - (y_count * geom.height_in)) / (y_count - 1);
  std::cout << "overlay_y_gap_in " << overlay_y_gap_in << "\n";

  // transform to inch coordinate system, origin at left
  contents = ("q "                                // push graphics stack
	      + std::format("{0:g} 0 0 {0:g} 0 0 cm ", PT_PER_IN)
	      );

  if (show_reg_marks)
  {
    contents += "q\n";
    contents += create_registration(page_width_in, page_height_in, cameo4_no_mat_reg_geometry);
    contents += "Q\n";
  }

  for (int y = 0; y < y_count; y++)
  {
    std::cout << "overlay " << y << "\n";
    double left = (page_width_in - geom.width_in) / 2.0;
    std::cout << "left " << left << "\n";
    double top = top_in + (y * (geom.height_in + overlay_y_gap_in));
    std::cout << "top " << top << "\n";
    double bottom = top + geom.height_in;
    std::cout << "bottom " << bottom << "\n";

    // transform to inch coordinate system, origin at bottom left
    // XXX why the heck do I need to subtract 1.75 from bottom for HP Voyager,
    // ? for SwissMicros???
    contents += ("q "
 		 + std::format("1 0 0 1 {0:g} {1:g} cm\n", left, bottom - 1.55));

    contents += create_overlay(geom, show_outlines, show_legends);

    contents += "Q\n";
  }

  contents += "Q\n";

  return pdf.newStream(contents);
  //return QPDFObjectHandle::newStream(&pdf, contents);
}


constexpr double letter_width_in = 8.5;
constexpr double letter_height_in = 11.0;

constexpr double letter_width_pt = letter_width_in * PT_PER_IN;
constexpr double letter_height_pt = letter_height_in * PT_PER_IN;


static void create_page(QPDFPageDocumentHelper &dh,
			std::string font_name,
			QPDFObjectHandle font_obj,
			RegistrationGeometry reg_geom,
			const OverlayGeometry& geom,
			bool do_outlines,
			bool do_reg_marks,
			bool do_legends)
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
		       reg_geom,
		       geom,
		       do_outlines,  // show_outlines
		       do_reg_marks, // show_reg_marks
		       do_legends);  // show legends

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
		       const RegistrationGeometry& reg_geom,
		       const OverlayGeometry& geom,
		       bool do_outlines,
		       bool do_reg_marks,
		       bool do_legends)
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
	      reg_geom,
	      geom,
	      do_outlines,
	      do_reg_marks,
	      do_legends);

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
      do_outlines  = true;
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
	     cameo4_no_mat_reg_geometry,
	     *geom,
	     do_outlines,
	     do_reg_marks,
	     do_legends);

  return 0;
}
