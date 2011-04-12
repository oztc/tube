===============
Tube Web Server
===============

A scalable synchronous server using pipeline architecture.  The pipeline architecture resemble the design of Glassfish Application Server.

Tube Web Server aims to be a scalable, flexible, extensible Web Server.  It was designed to make developer writing web logic easily within the Web Server as easy as possible, and at the same time, keeping fairly good performance and scalability.

Platform Support
----------------

Currently, Tube Web Server is under heavy development.  The primary platform to support is Linux 2.6.  FreeBSD 6+ and Solaris 10+ is also supported, but under heavy development currently.

Build
-----

The Tube Web Server is written in C/C++. To build Tube Web Server, you'll need the following tools and libraries installed.
 
 * ``SCons`` > 2.0. available at `<http://www.scons.org/>`_
 * boost library with the following component:
    * ``boost::thread``
    * ``boost::function``
    * ``boost::smart_ptr``
    * ``boost::xpressive``
 * ``yaml-cpp``
 * ``ragel`` available at `<http://www.complang.org/ragel/>`_

To Build Tube, simply run ::

    % scons 

If you want the release version (which doesn't have log and debug symbol) ::
    
    % scons release=1
    
After building process succeeded, run the following to install ::

    % scons install

If you want to install in a sandbox directory (or DESTDIR in automake terminology), use ::

    % scons install --install-sandbox=<dir>

Usage
-----

After installation, you should be able to use ``tube-server``. Make sure that ``tube-server`` is in your ``PATH`` and ``libtube.so`` and ``libtube-web.so`` are able to load by system. ::

    tube-server -c config_file [ -m module_path -u uid]

 * ``config_file`` is the configuration file. This is required.
 * ``module_path`` is the path containing all the modules. This is optional.
 * ``uid`` is the uid if you want ``tube`` switch uid before launch. This is optional

There's a sample config file in ``test/test-yaml.conf``.

Author
------

Mike Qin <mikeandmore AT gmail.com>
