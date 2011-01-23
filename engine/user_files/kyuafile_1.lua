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


-- \file engine/kyuafile-1.lua
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
-- AtfTestProgram {name="foo_test"}
-- AtfTestProgram {name="bar_test"}
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


-- Joins two paths, skipping the former if it is '.'.
--
-- \param p1 string, The initial path.
-- \param p2 string, The path to append to p1.
--
-- \return The concatenation of p1 with p2.
local function join_paths(p1, p2)
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
-- \param properties table, The properties describing the test program.
--     The allowed keys and their types are: name:string.
function AtfTestProgram(properties)
   table.insert(TEST_PROGRAMS, properties.name)
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
   local abs_file = join_paths(fs.dirname(init.get_filename()), file)

   local env = init.run(abs_file)

   local syntax = env.init.get_syntax()
   if syntax.format == "kyuafile" then
      if syntax.version == 1 then
         for _, test_program in ipairs(env.kyuafile.TEST_PROGRAMS) do
            if fs.is_absolute(test_program) then
               table.insert(TEST_PROGRAMS, test_program)
            else
               table.insert(TEST_PROGRAMS,
                            join_paths(fs.dirname(file), test_program))
            end
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


-- Sets globals for commonly-used and required module entities.
--
-- \post The global environment is modified to include an entry for the module
-- itself as well as entries for the functions that the user will need in his
-- configuration files.
function export()
   _G.kyuafile = P

   _G.AtfTestProgram = AtfTestProgram
   _G.include = include
end


return P
