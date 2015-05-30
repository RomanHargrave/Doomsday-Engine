#!/usr/bin/env python2.7
# This Python script will create a set of PK3 files that contain the files
# that Doomsday needs at runtime. The PK3 files are organized using the
# traditional data/ and defs/ structure.

import sys, os, os.path, zipfile

if len(sys.argv) < 2:
    print "Usage: %s pk3-target-dir" % sys.argv[0]
    print "(run in build/scripts/)"
    sys.exit(0)

# Check quiet flag.
quietMode = False
if '--quiet' in sys.argv:
    sys.argv.remove('--quiet')
    quietMode = True

deng_dir = os.path.join('..', '..', 'apps')
target_dir = os.path.abspath(sys.argv[1])

class Pack:
    def __init__(self):
        self.files = [] # tuples

    def add_files(self, fileNamesArray):
        self.files += fileNamesArray

    def msg(self, text):
        if not quietMode: print text

    def create(self, name):
        full_name = os.path.join(target_dir, name)
        self.msg("Creating %s as %s..." % (os.path.normpath(name), os.path.normpath(full_name)))

        pk3 = zipfile.ZipFile(full_name, 'w', zipfile.ZIP_DEFLATED)

        for src, dest in self.files:
            full_src = os.path.join(deng_dir, src)
            # Is this a file or a folder?
            if os.path.isfile(full_src):
                # Write the file as is.
                self.msg("writing %s as %s" % (os.path.normpath(full_src), os.path.normpath(dest)))
                pk3.write(full_src, dest)
            elif os.path.isdir(full_src):
                # Write the contents of the folder recursively.
                def process_dir(path, dest_path):
                    self.msg("processing %s" % os.path.normpath(path))
                    for file in os.listdir(path):
                        real_file = os.path.join(path, file)
                        if file[0] == '.':
                            continue # Ignore these.
                        if os.path.isfile(real_file):
                            if not quietMode:
                                self.msg("writing %s as %s" % (os.path.normpath(real_file),
                                         os.path.normpath(os.path.join(dest_path, file))))
                            pk3.write(real_file, os.path.join(dest_path, file))
                        elif os.path.isdir(real_file):
                            process_dir(real_file,
                                        os.path.join(dest_path, file))
                process_dir(full_src, dest)

        # Write it out.
        print "Created %s (with %i files)." % (os.path.normpath(full_name), len(pk3.namelist()))
        pk3.close()

# First up, doomsday.pk3.
# Directory contents added recursively.
p = Pack()
p.add_files(
    [ ('client/data', 'data') ] )
p.create('doomsday.pk3')

# libdoom.pk3
p = Pack()
p.add_files(
    [ ('plugins/doom/defs', 'defs/jdoom'),
      ('plugins/doom/data/chex.mapinfo',           'data/jdoom/chex.mapinfo'),
      ('plugins/doom/data/doom1-share.mapinfo',    'data/jdoom/doom1-share.mapinfo'),
      ('plugins/doom/data/doom1-ultimate.mapinfo', 'data/jdoom/doom1-ultimate.mapinfo'),
      ('plugins/doom/data/doom1.mapinfo',          'data/jdoom/doom1.mapinfo'),
      ('plugins/doom/data/doom2-plut.mapinfo',     'data/jdoom/doom2-plut.mapinfo'),
      ('plugins/doom/data/doom2-tnt.mapinfo',      'data/jdoom/doom2-tnt.mapinfo'),
      ('plugins/doom/data/doom2.mapinfo',          'data/jdoom/doom2.mapinfo'),
      ('plugins/doom/data/hacx.mapinfo',           'data/jdoom/hacx.mapinfo'),
      ('plugins/doom/data/conhelp.txt', 'data/jdoom/conhelp.txt'),
      ('plugins/doom/data/lumps', '#.basedata') ] )
p.create('libdoom.pk3')

# libheretic.pk3
p = Pack()
p.add_files(
    [ ('plugins/heretic/defs', 'defs/jheretic'),
      ('plugins/heretic/data/heretic-ext.mapinfo',   'data/jheretic/heretic-ext.mapinfo'),
      ('plugins/heretic/data/heretic-share.mapinfo', 'data/jheretic/heretic-share.mapinfo'),
      ('plugins/heretic/data/heretic.mapinfo',       'data/jheretic/heretic.mapinfo'),
      ('plugins/heretic/data/conhelp.txt', 'data/jheretic/conhelp.txt'),
      ('plugins/heretic/data/lumps', '#.basedata') ] )
p.create('libheretic.pk3')

# libhexen.pk3
p = Pack()
p.add_files(
    [ ('plugins/hexen/defs', 'defs/jhexen'),
      ('plugins/hexen/data/hexen-dk.mapinfo', 'data/jhexen/hexen-dk.mapinfo'),
      ('plugins/hexen/data/hexen.mapinfo',    'data/jhexen/hexen.mapinfo'),
      ('plugins/hexen/data/conhelp.txt',      'data/jhexen/conhelp.txt'),
      ('plugins/hexen/data/lumps', '#.basedata') ] )
p.create('libhexen.pk3')

# libdoom64.pk3
p = Pack()
p.add_files(
    [ ('plugins/doom64/defs', 'defs/jdoom64'),
      ('plugins/doom64/data/doom64.mapinfo', 'data/jdoom64/doom64.mapinfo'),
      ('plugins/doom64/data/conhelp.txt', 'data/jdoom64/conhelp.txt'),
      ('plugins/doom64/data/lumps', '#.basedata') ] )
p.create('libdoom64.pk3')
