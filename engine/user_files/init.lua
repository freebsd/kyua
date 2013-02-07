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


-- \file engine/user_files/init.lua
-- Code to initialize the processing of a specific Kyua configuration file.
--
-- A Kyua configuration file has always the following structure:
--
-- ---- cut here ----
-- syntax("<format>", <version>)
--
-- ... anything else as defined by the specific format-version ...
-- ---- cut here ----
--
-- In order to process a file like the above, the caller must follow the
-- protocol described in the run() function.
--
-- TODO(jmmv): Ideally, this module would be written completely in C++.  This
-- would allow us to avoid importing *any* global modules by default (the ones
-- required for the code below to work).  Required modules (such as table, fs,
-- etc.) would be imported by syntax() depending on the appropriate format.


local P = {}
setmetatable(P, {__index = _G})
setfenv(1, P)


-- table, A copy of the pristine global environment (as it was on entry).
--
-- At this point, the environment contains only those keys defined on entry:
-- i.e. the entities imported into the global namespace by the host application.
--
-- We must store a copy of the global environment state at this point before we
-- add new entries to it, so that when we chainload another file, we can set up
-- a completely clean state for it.
local ENV = {}
do
   for key, value in pairs(_G) do
      ENV[key] = value
   end
   setmetatable(ENV, getmetatable(_G))
end


-- Executes a file in a subenvironment.
--
-- \param file string, The file to execute.
-- \param env table, The environment in which to execute the file.
--
-- \return Whatever value the execution of the file yields.
local function dofile_in_env(file, env)
   setfenv(1, env)
   local chunk, error_message = loadfile(file)
   if chunk == nil then
      error(error_message)
   else
      setfenv(chunk, env)
      return chunk()
   end
end


-- string, Path to the Lua modules installed by Kyua.
local LUADIR = nil


-- string, Path to the file being processed.
--
-- Use get_filename() to query.
local FILENAME = nil


-- table, Syntax definition for the current configuration file.
--
-- The 'format' member is a string and 'version' is an integer.
--
-- Use get_syntax() to query.
local SYNTAX = {format=nil, version=nil}


-- Initializes internal state.
--
-- This function must be called as the very first thing after including this
-- module.  The call is done either by the C++ host application or by the run()
-- function below when loading a seconary file.
--
-- \pre The function has not been called yet.
-- \post LUADIR and FILENAME are updated.
--
-- \param luadir string, The path to the Lua modules installed by Kyua.
-- \param filename string, The path to the file being processed.
function bootstrap(luadir, filename)
   assert(not LUADIR)
   LUADIR = luadir

   assert(not FILENAME)
   FILENAME = filename

   logging.debug(string.format("Initialized file '%s'", FILENAME))
end


-- Returns the name of the file being processed.
--
-- \return string, An absolute or relative path name.
function get_filename()
   return FILENAME
end


-- Returns the syntax of the file being processed.
--
-- \return table, A table with fields 'format' and 'version'.
function get_syntax()
   if not SYNTAX.format or not SYNTAX.version then
      error(string.format("Syntax not defined in file '%s'", FILENAME))
   end

   return SYNTAX
end


-- Executes a file in a sandbox and returns its environment after execution.
--
-- The sandbox is constructed using the copy of the pristine global environment
-- stored in the ENV variable.  See its description for more details on what it
-- contains.
--
-- \param file string, The file to execute.
--
-- \return table, The environment after the file has executed.
function run(file)
   logging.debug(string.format("Running file '%s'", file))

   local env = {}
   setmetatable(env, {__index = ENV})
   env._G = env

   local module = dofile_in_env(fs.join(LUADIR, "init.lua"), env)
   module.bootstrap(LUADIR, file)
   module.export()

   dofile_in_env(file, env)

   return env
end


-- Sets the syntax for the current file being processed.
--
-- This must be called by the configuration file as its very first operation to
-- bring into its namespace the required modules and global entities.
--
-- \pre The function has not been called yet.
-- \post The environment of the caller contains any global definitions specified
--     by the requested (format, version) pair.
-- \post The SYNTAX variable contains the details about the requested (format,
--     version) pair.
--
-- \param format string, The format required by the file.
-- \param version integer, The version of the format required by the file.
function syntax(format, version)
   assert(not SYNTAX.format and not SYNTAX.version,
          "Cannot call syntax() more than once in a single file")

   logging.debug(string.format("Setting syntax to '%s', version %d", format,
                               version))
   if format == "kyuafile" then
      if version == 1 then
         local module = dofile_in_env(fs.join(LUADIR, "kyuafile_1.lua"),
                                      getfenv(2))
         module.export()
      else
         error(string.format("Syntax request error: unknown version %d for " ..
                             "format '%s'", version, format))
      end
   else
      error(string.format("Syntax request error: unknown format '%s'", format))
   end

   SYNTAX.format = format
   SYNTAX.version = version
end


-- Sets globals for commonly-used and required module entities.
--
-- \post The global environment is modified to include an entry for the module
-- itself as well as entries for the functions that the user will need in his
-- configuration files.
function export()
   _G.init = P

   _G.syntax = syntax
end


return P
