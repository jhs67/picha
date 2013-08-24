{
	'targets': [
		{
			'target_name': 'picha',
			'sources': [
				'src/picha.cc',
				'src/pngcodec.cc',
				'src/jpegcodec.cc',
				'src/colorconvert.cc',
				'src/resize.cc',
			],
			'cflags': [
				'<!@(pkg-config libpng --cflags)',
				'-w',
			],
			'ldflags': [
				'<!@(pkg-config libpng --libs-only-L --libs-only-other)',
			],
			'libraries': [
				'<!@(pkg-config libpng --libs-only-l)',
				'-ljpeg',
			],
			'xcode_settings': {
				'OTHER_CFLAGS': [ '<!@(pkg-config libpng --cflags)', '-I /opt/local/include' ],
				'OTHER_LDFLAGS': [ '<!@(pkg-config libpng --libs-only-L --libs-only-other)', '-L/opt/local/lib' ],
			},
		},
	],
}
