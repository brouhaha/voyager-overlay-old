# Copyright 2023 Eric Smith
# SPDX-License-Identifier: GPL-3.0-only

env = Environment(CXXFLAGS = "-g --std=c++20")

env.ParseConfig('pkg-config --cflags --libs freetype2')

libs = ["qpdf", "boost_program_options"]



voyager_overlay_sources = ['voyager-overlay.cpp', 'content_stream_string.cpp']

env.Program('voyager-overlay', voyager_overlay_sources, LIBS = libs)
