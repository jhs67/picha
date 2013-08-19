{
	'targets': [
		{
			'target_name': 'picha',
			'sources': [
				'src/picha.cc',
				'src/pngcodec.cc',
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
			],
			'xcode_settings': {
				'OTHER_CFLAGS': [ '<!@(pkg-config libpng --cflags)' ],
				'OTHER_LDFLAGS': [ '<!@(pkg-config libpng --libs-only-L --libs-only-other)' ],
			},
		},
	],
}
