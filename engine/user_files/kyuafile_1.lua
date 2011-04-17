-- Copyright 2011 Google Inc.
-- All rights reserved.
--
-- Redistribution and use in source and binary forms, with or without
-- modification, are permitted provided that the following conditions are
-- met:
--
-- * Redistributions of source code must retain the above copyright
--   notice, this list of conditions and the following disclaimer.
-- * Redistributions in binary form must reproduce the above copyright
--   notice, this list of conditions and the following disclaimer in the
--   documentation and/or other materials provided with the distribution.
-- * Neither the name of Google Inc. nor the names of its contributors
--   may be used to endorse or promote products derived from this software
--   without specific prior written permission.
--
-- THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
-- "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
-- LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
-- A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
-- OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
-- SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
-- LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
-- DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
-- THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
-- (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
-- OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


-- \file engine/user_files/kyuafile_1.lua
-- Functions to ease the definition of Kyuafiles.
--
-- This module provides a set of public functions to simplify the definition of
-- Kyuafiles.  These functions are put in the global namespace by calling the
-- export() function, something that is automatically done by the init.syntax()
-- method.
--
-- A typical Kyuafile looks like this:
--
-- ---- cut here ----
-- syntax("kyuafile", 1)
--
-- atf_test_program{name="foo_test"}
-- atf_test_program{name="bar_test"}
-- ... more test program definitions ...
--
-- include("dir1/Kyuafile")
-- include("dir2/Kyuafile")
-- ... more inclusions ...
-- ---- cut here ----
--
-- Note that included files do NOT share the environment.  They are run in a
-- sandbox and thus arbitrary state cannot be passed across inclusions.
--
-- Any non-exported functions should not be required by the user-defined
-- scripts.  These are only used internally for communication among different
-- Kyuafiles or with the host application.


local P = {}
setmetatable(P, {__index = _G})
setfenv(1, P)


-- array(string), Collection of test programs encountered so far.
TEST_PROGRAMS = {}


-- string, The name of the test suite to which the test programs belong to.
TEST_SUITE = nil


-- Copies a table, but not the contents.
--
-- \param in_table table, The input table.
--
-- \return table, The copied table.
local function copy_table(in_table)
   out_table = {}
   for key, value in pairs(in_table) do
      out_table[key] = value
   end
   setmetatable(out_table, getmetatable(in_table))
   return out_table
end


-- Concatenates two paths (using special semantics).
--
-- This function is designed to concatenate an arbitrary p1 to an arbitrary
-- (i.e. not necessarily relative) p2.  p1 is discarded if it is '.' or if p2
-- is absolute.
--
-- \param p1 string, The initial path.
-- \param p2 string, The path to append to p1.
--
-- \return The concatenation of p1 with p2.
local function mix_paths(p1, p2)
   assert(not fs.is_absolute(p2),
          string.format("Got unexpected absolute path '%s'", p2))
   if p1 == "." then
      return p2
   else
      return fs.join(p1, p2)
   end
end


-- Registers a test program that follows the ATF test interface.
--
-- Use this as a table constructor.
--
-- \post TEST_PROGRAMS contains a new entry for the test program.
--
-- \param in_properties table, The properties describing the test program.
--     The allowed keys and their types are: name:string, test_suite:string.
function atf_test_program(in_properties)
   local properties = copy_table(in_properties)
   if not properties.test_suite then
      assert(TEST_SUITE, "No global test suite defined and no test suite " ..
             "provided in the test program definition (cannot add test " ..
             "program '" .. properties.name .. "')")
      properties.test_suite = TEST_SUITE
   end
   assert(fs.basename(properties.name) == properties.name,
          string.format("Test program '%s' cannot contain path components",
                        properties.name))
   table.insert(TEST_PROGRAMS, properties)
end


-- Processes a new Kyuafile.
--
-- This function runs the provided Kyuafile in a completely separate
-- environment.  Upon termination, this function collects the definitions
-- performed inside the included Kyuafile and merges them with the state
-- currently defined by the caller Kyuafile.
--
-- \post TEST_PROGRAMS has been updated to include the list of test programs
--     defined by the included Kyuafile.
--
-- \param relative_file string, The Kyuafile to process, relative to the
--     Kyuafile being processed.
function include(file)
   assert(not fs.is_absolute(file),
          string.format("Cannot include '%s' using an absolute path", file))
   local abs_file = mix_paths(fs.dirname(init.get_filename()), file)

   local env = init.run(abs_file)

   local syntax = env.init.get_syntax()
   if syntax.format == "kyuafile" then
      if syntax.version == 1 then
         for _, raw_test_program in ipairs(env.kyuafile.TEST_PROGRAMS) do
            local test_program = copy_table(raw_test_program)
            test_program.name = mix_paths(fs.dirname(file), test_program.name)
            table.insert(TEST_PROGRAMS, test_program)
         end
      else
         error(string.format("Inclusion of '%s' by '%s' failed: unsupported " ..
                             "kyuafile version %d", file, init.get_filename(),
                              syntax.version))
      end
   else
      error(string.format("Inclusion of '%s' by '%s' failed: unsupported " ..
                          "format %s", file, init.get_filename(),
                          syntax.format))
   end
end


-- Defines the default test suite for the test programs in the Kyuafile.
--
-- A test program can override the default value by passing a 'test_suite'
-- property to its definition.  (I'm not sure if this is a wise thing to offer
-- though.)
--
-- Note that this value is not passed recursively to included Kyuafiles on
-- purpose because we want such Kyuafiles to be self-contained.
--
-- \post The default test suite for the file has been set; i.e.
--     TEST_SUITE = name.
--
-- \param name string, The name of the test suite.
function test_suite(name)
   assert(not TEST_SUITE, "Test suite already defined; cannot call " ..
          "test_suite twice")
   TEST_SUITE = name
end


-- Sets globals for commonly-used and required module entities.
--
-- \post The global environment is modified to include an entry for the module
-- itself as well as entries for the functions that the user will need in his
-- configuration files.
function export()
   _G.kyuafile = P

   _G.atf_test_program = atf_test_program
   _G.include = include
   _G.test_suite = test_suite
end


return P
