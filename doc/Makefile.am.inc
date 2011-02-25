# Copyright 2011 Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
# * Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
# * Neither the name of Google Inc. nor the names of its contributors
#   may be used to endorse or promote products derived from this software
#   without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

info_TEXINFOS = doc/kyua-cli.texi
doc_kyua_cli_TEXINFOS  = doc/copying.texi
doc_kyua_cli_TEXINFOS += doc/overview.texi
doc_kyua_cli_TEXINFOS += doc/test_interface.texi
doc_kyua_cli_TEXINFOS += doc/troubleshooting.texi

man_MANS = doc/kyua.1
CLEANFILES = doc/kyua.1
EXTRA_DIST += doc/kyua.1.in

doc/kyua.1: $(srcdir)/doc/kyua.1.in
	test -d doc || mkdir -p doc
	sed -e 's,__DOCDIR__,$(docdir),g' \
	    -e 's,__INFODIR__,$(infodir),g' \
	    -e 's,__TESTSDIR__,$(testsdir),g' \
	    <$(srcdir)/doc/kyua.1.in >doc/kyua.1.tmp
	mv doc/kyua.1.tmp doc/kyua.1
