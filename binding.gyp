{
	'variables': {
		'with_jpeg': 'yes',
		'with_png': '<!(pkg-config --exists libpng && echo yes || echo no)',
		'with_tiff': '<!(pkg-config --exists libtiff-4 && echo yes || echo no)',
	},
	'targets': [
		{
			'target_name': 'picha',
			'sources': [
				'src/picha.cc',
				'src/resize.cc',
				'src/writebuffer.cc',
				'src/colorconvert.cc',
			],
			'cflags': [
				'-w',
			],
			'xcode_settings': {
				'OTHER_CFLAGS': [ '-I/opt/local/include' ],
				'OTHER_LDFLAGS': [ '-L/opt/local/lib' ],
			},
			'conditions': [
				['with_png == "yes"', {
					'sources': [
						'src/pngcodec.cc',
					],
					'defines': [
						'WITH_PNG',
					],
					'cflags': [
						'<!@(pkg-config libpng --cflags)',
					],
					'ldflags': [
						'<!@(pkg-config libpng --libs-only-L --libs-only-other)',
					],
					'libraries': [
						'<!@(pkg-config libpng --libs-only-l)',
					],
					'xcode_settings': {
						'OTHER_CFLAGS': [ '<!@(pkg-config libpng --cflags)' ],
						'OTHER_LDFLAGS': [ '<!@(pkg-config libpng --libs-only-L --libs-only-other)' ],
					},
				}],
				['with_jpeg == "yes"', {
					'sources': [
						'src/jpegcodec.cc',
					],
					'defines': [
						'WITH_JPEG',
					],
					'libraries': [
						'-ljpeg',
					],
				}],
				['with_tiff == "yes"', {
					'sources': [
						'src/tiffcodec.cc',
					],
					'defines': [
						'WITH_TIFF',
					],
					'cflags': [
						'<!@(pkg-config libtiff-4 --cflags)',
					],
					'ldflags': [
						'<!@(pkg-config libtiff-4 --libs-only-L --libs-only-other)',
					],
					'libraries': [
						'<!@(pkg-config libtiff-4 --libs-only-l)',
					],
					'xcode_settings': {
						'OTHER_CFLAGS': [ '<!@(pkg-config libtiff-4 --cflags)' ],
						'OTHER_LDFLAGS': [ '<!@(pkg-config libtiff-4 --libs-only-L --libs-only-other)' ],
					},
				}],
			],
		},
	],
}
