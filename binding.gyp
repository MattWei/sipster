{
  'targets': [
    {
      'target_name': 'sipster',
      'sources': [
        'src/binding.cc',
        'src/SIPSTERAccount.cc',
        'src/SIPSTERCall.cc',
        'src/SIPSTERMedia.cc',
        'src/SIPSTERTransport.cc',
        'src/SIPSTERBuddy.cc',
        'src/hplayer.cpp'
      ],
      'include_dirs': [
        "src",
        "<!(node -e \"require('nan')\")"
      ],
      'conditions': [
        [ 'OS!="win"', {
          'cflags_cc': [
            '<!@(pkg-config --cflags --libs /home/weiminji/workspace/sources/libpjsip/linux/lib/lib/pkgconfig/libpjproject.pc)',
            '-fexceptions',
            '-Wno-maybe-uninitialized',
          ],
          'libraries': [
            '<!@(pkg-config --cflags --libs /home/weiminji/workspace/sources/libpjsip/linux/lib/lib/pkgconfig/libpjproject.pc)',
          ],
        }],
        [ 'OS=="mac"', {
          'xcode_settings': {
            'OTHER_CFLAGS': [
              '-fexceptions',
              '-frtti',
            ],
          },

          # begin gyp stupidity workaround =====================================
          'ldflags!': [
            '-framework CoreAudio',
          ],
          'libraries!': [
            'CoreServices', 
            'AudioUnit',
            'AudioToolbox',
            'Foundation',
            'AppKit',
            'QTKit',
            'QuartzCore',
            'OpenGL',
          ],
          'libraries': [
            'CoreAudio.framework',
            'CoreServices.framework',
            'AudioUnit.framework',
            'AudioToolbox.framework',
            'Foundation.framework',
            'AppKit.framework',
            'QTKit.framework',
            'QuartzCore.framework',
            'OpenGL.framework',
          ],
          # end gyp stupidity workaround =======================================

        }],
      ],
    },
  ],
}
