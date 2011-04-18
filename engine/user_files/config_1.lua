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


-- \file engine/user_files/config_1.lua
-- Functions to ease the definition of Kyua configuration files.
--
-- This module provides a set of public functions to simplify the definition of
-- Kyuafiles.  These functions are put in the global namespace by calling the
-- export() function, something that is automatically done by the init.syntax()
-- method.
--
-- A typical Kyuafile looks like this:
--
-- ---- cut here ----
-- syntax("config", 1)
--
-- ... variable definitions ...
-- ---- cut here ----


local P = {}
setmetatable(P, {__index = _G})
setfenv(1, P)


TEST_SUITES = {}


-- Defines a configuration variable for a particular test suite.
--
-- \post TEST_SUITES[test_suite][name] == value
--
-- \param test_suite string, The name of the test suite for which a property is
--     being set.
-- \param name string, The name of the property to set.
-- \param value boolean|number|string, The value of the property.
function test_suite_var(test_suite, name, value)
   assert(type(test_suite) == "string")
   assert(type(name) == "string")
   assert(type(value) == "boolean" or type(value) == "number" or
          type(value) == "string")

   if TEST_SUITES[test_suite] == nil then
      TEST_SUITES[test_suite] = {}
   end
   TEST_SUITES[test_suite][name] = value

   logging.info(string.format("Set %s = %s for test suite %s", name, value,
                              test_suite))
end


-- Sets globals for commonly-used and required module entities.
--
-- \post The global environment is modified to include an entry for the module
-- itself as well as entries for the functions that the user will need in his
-- configuration files.
function export()
   _G.config = P

   _G.test_suite_var = test_suite_var
end


return P
