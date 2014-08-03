# Welcome to the Kyua project!

Kyua is a **testing framework** for operating systems.

Kyua features an **expressive test suite definition language**, a **safe
runtime engine** for test suites and a **powerful report generation
engine**.

Kyua is for **both developers *and* users**, from the developer applying a
simple fix to a library to the system administrator deploying a new release
on a production machine.

Kyua is **able to execute test programs written with a plethora of testing
libraries and languages**.  The library of choice is
[ATF](https://github.com/jmmv/atf/), for which Kyua was originally
designed, but simple, framework-less test programs and TAP-compliant test
programs can also be executed through Kyua.

[Read more about Kyua in the About wiki page.](../../wiki/About)

## Download

Formal releases for source files are available for download from GitHub.

* [kyua-testers 0.3](../../releases/tag/kyua-testers-0.3), released on
  August 8th, 2014.  *Required*.
* [kyua-cli 0.9](../../releases/tag/kyua-cli-0.9), released on August
  8th, 2014.  *Required*.

## Installation

You are encouraged to install binary packages for your operating system
wherever available:

* Fedora 20 and above: install the `kyua-cli` package with `yum install
  kyua-cli`.

* FreeBSD 10.0 and above: install the `kyua-cli` package with `pkg install
  kyua-cli`.

* NetBSD with pkgsrc: install the `pkgsrc/devel/kyua-cli` package.

Should you want to build and install Kyua from the source tree provided
here, follow the instructions in the
[INSTALL file](kyua-cli/INSTALL).

You should also install the ATF libraries to assist in the development of
test programs.  To that end, see the
[ATF project page](https://github.com/jmmv/atf/).

## Source tree

This source tree contains all the modules that form the Kyua project.
These modules match the list of downloads above, and are, in
reverse-dependency order:

* `kyua-testers`: Scriptable interfaces to interact with test programs
  of various kinds.

* `kyua-cli`: Runtime and reporting engine, including the command-line
  interface to Kyua.

## Support

Please use the
[kyua-discuss mailing list](https://groups.google.com/forum/#!forum/kyua-discuss)
for any support inquiries.
