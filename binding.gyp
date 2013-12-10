{
	'variables': {
		'with_png': '<!(pkg-config --exists libpng && echo yes || echo no)',
		'with_jpeg': 'yes',
	},
	'targets': [
		{
			'target_name': 'picha',
			'sources': [
				'src/picha.cc',
				'src/colorconvert.cc',
				'src/resize.cc',
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
			],
		},
	],
}
