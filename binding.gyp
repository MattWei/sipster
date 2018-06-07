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
        "<!(node -e \"require('nan')\")",
        "../pjsip/include"
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
	      [ 'OS=="win"', {
          "link_settings": {
            "libraries": [
              "-llibpjproject-Static.lib",
              "-lwsock32.lib",
              "-lws2_32.lib",
              "-ldsound.lib"
            ]
          },
          "configurations": {
                            "Debug": {
                                "msvs_settings": {
                                    "VCCLCompilerTool": {
                                        "ExceptionHandling": "0",
                                        "AdditionalOptions": [
                                            "/MP /EHsc"
                                        ]
                                    },
                                    "VCLibrarianTool": {
                                        "AdditionalOptions": [
                                            "/LTCG"
                                        ]
                                    },
                                    "VCLinkerTool": {
                                        "LinkTimeCodeGeneration": 1,
                                        "LinkIncremental": 1,
                                        "AdditionalLibraryDirectories": [
                                            "D:/workspace/ionic_project/pjsip/debug/"
                                        ]
                                    }
                                }
                            },
                            "Release": {
                                "msvs_settings": {
                                    "VCCLCompilerTool": {
                                        "RuntimeLibrary": 0,
                                        "Optimization": 3,
                                        "FavorSizeOrSpeed": 1,
                                        "InlineFunctionExpansion": 2,
                                        "WholeProgramOptimization": "true",
                                        "OmitFramePointers": "true",
                                        "EnableFunctionLevelLinking": "true",
                                        "EnableIntrinsicFunctions": "true",
                                        "RuntimeTypeInfo": "false",
                                        "ExceptionHandling": "0",
                                        "AdditionalOptions": [
                                            "/MP /EHsc"
                                        ]
                                    },
                                    "VCLibrarianTool": {
                                        "AdditionalOptions": [
                                            "/LTCG"
                                        ]
                                    },
                                    "VCLinkerTool": {
                                        "LinkTimeCodeGeneration": 1,
                                        "OptimizeReferences": 2,
                                        "EnableCOMDATFolding": 2,
                                        "LinkIncremental": 1,
                                        "AdditionalLibraryDirectories": [
                                            "D:/workspace/ionic_project/pjsip/release/"
                                        ]
                                    }
                                }
                            }
                        },
          'include_dirs': [
              "D:/workspace/ionic_project/pjsip/include/",
          ],
          # end gyp stupidity workaround =======================================

        }],
      ],
    },
  ],
}
