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


-- newindex metatable method for a test suite table.
--
-- \param table table, The table containing all the test-suite-specific
--     configuration variables.
-- \param key string, The name of the configuration variable to be set.
-- \param value boolean|number|string, The new value for the configuration
--     variable.
function test_suite_newindex(table, key, value)
   local test_suite_name = getmetatable(table)._test_suite_name

   assert(type(key) == "string",
          string.format("Key '%s' not a string while indexing test suite '%s'",
                        key, test_suite_name))
   assert(type(value) == "boolean" or type(value) == "number" or
          type(value) == "string",
          string.format("Invalid type for property '%s' in test suite '%s'",
                        key, test_suite_name))

   rawset(table, key, value)

   if type(value) == "boolean" then
      str_value = value and "true" or "false"
   else
      str_value = tostring(value)
   end
   logging.info(string.format("Set %s = %s for test suite %s", key, str_value,
                              test_suite_name))
end


-- index metatable method for the TEST_SUITES table.
--
-- This method creates a new subtable of TEST_SUITES every time it is invoked on
-- a non-existent key.  This way we ensure that all elements of TEST_SUITES are
-- tables on their own and represent a key/value mapping on themselves.
--
-- \param table table, The table containing all the tables representing
--     test-suite-specific configuration variables..
-- \param key string, The name of the test suite to get (or create if it has not
--     been accessed yet).
--
-- \return table, The table representing the test-suite-specific variables for
-- the test suite named 'key'.
function test_suites_index(table, key)
   assert(type(key) == "string", "Test suite name must be a string")

   value = rawget(table, key)
   if value == nil then
      value = {}
      setmetatable(value, {__newindex = test_suite_newindex,
                           _test_suite_name = key})
      rawset(table, key, value)
   end

   assert(type(value) == "table")
   return value
end


-- newindex metatable method for the TEST_SUITES table.
--
-- We disallow users to call this method on the global TEST_SUITES table because
-- we do not want them to assign arbitrary values to members of this table.  The
-- 'index' method will create default objects instead upon demand.
--
-- \param table table, The table containing all the test-suite-specific
--     configuration variables.
-- \param key string, The name of the configuration variable to be set.
-- \param value boolean|number|string, The new value for the configuration
--     variable.
function test_suites_newindex(unused_table, unused_key, unused_value)
   error('Cannot directly set values of test_suites; index them instead')
end


-- Definition of all test-suite-specific configuration variables.
--
-- This table is tweaked to force all of its elements to be other tables, which
-- in turn represent the configuration variables of a single test suite.
TEST_SUITES = {}
setmetatable(TEST_SUITES, {
    __index = test_suites_index,
    __newindex = test_suites_newindex,
})


-- Sets globals for commonly-used and required module entities.
--
-- \post The global environment is modified to include an entry for the module
-- itself as well as entries for the functions that the user will need in his
-- configuration files.
function export()
   _G.config = P

   _G.test_suites = TEST_SUITES
end


return P
