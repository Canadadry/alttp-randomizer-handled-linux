// Usage: NODE_PATH=$(npm root -g) node mockup_render.js OUT SVG...
// Renders each SVG, opened as file://, in headless Chromium at 640x480 into OUT/<name>.png and prints
// one "mockups: <name>: <problem>" line per failure, exit 1 if any: a request that is not file:
// (aborted), no loaded "Press Start 2P" face, a <text> whose computed length is not its characters
// times its computed font-size within 0.5 px (checked only once the face is loaded). Proves itself
// first on four planted SVGs beside a byte copy of data/PressStart2P.ttf. Also writes OUT/<name>.texts.txt:
// per <text> outside defs, tab-separated, its getBBox x y width height, its computed fill rrggbb, its text.
'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');

const FAMILY = 'Press Start 2P';
const TOLERANCE = 0.5;
const FONT = path.join(__dirname, '..', '..', '..', 'data', 'PressStart2P.ttf');

if (!process.env.PLAYWRIGHT_BROWSERS_PATH && fs.existsSync('/opt/pw-browsers'))
	process.env.PLAYWRIGHT_BROWSERS_PATH = '/opt/pw-browsers';

let chromium;
try {
	({ chromium } = require('playwright'));
} catch (e) {
	console.log('setup: the playwright module is not found (NODE_PATH=$(npm root -g), npm install -g playwright)');
	process.exit(1);
}

async function check(browser, file, out) {
	const name = path.basename(file, '.svg');
	const problems = [];
	const page = await browser.newPage({ viewport: { width: 640, height: 480 } });
	try {
		await page.route('**/*', route => {
			const url = route.request().url();
			if (url.startsWith('file:'))
				return route.continue();
			problems.push(`external request ${url}`);
			return route.abort();
		});
		await page.goto('file://' + path.resolve(file), { waitUntil: 'load' });
		const seen = await page.evaluate(async family => {
			await document.fonts.ready;
			const faces = [];
			document.fonts.forEach(f => faces.push({ family: f.family.replace(/["']/g, ''), status: f.status }));
			const loaded = faces.some(f => f.family === family && f.status === 'loaded');
			const texts = [...document.querySelectorAll('text')]
				.filter(t => !t.closest('defs'))
				.map(t => {
					const box = t.getBBox();
					const rgb = getComputedStyle(t).fill.match(/\d+/g) || [];
					return {
						text: t.textContent.replace(/[\r\n\t]/g, '').replace(/ +/g, ' ').trim(),
						length: t.getComputedTextLength(),
						size: parseFloat(getComputedStyle(t).fontSize),
						box: [box.x, box.y, box.width, box.height],
						fill: rgb.slice(0, 3).map(c => Number(c).toString(16).padStart(2, '0')).join(''),
					};
				});
			return { loaded, texts };
		}, FAMILY);
		if (!seen.loaded) {
			problems.push(`font ${FAMILY} not loaded`);
		} else {
			for (const t of seen.texts) {
				const want = [...t.text].length * t.size;
				if (Math.abs(t.length - want) > TOLERANCE)
					problems.push(`text "${t.text}" is ${t.length} px wide, not ${want}`);
			}
		}
		fs.mkdirSync(out, { recursive: true });
		await page.screenshot({ path: path.join(out, name + '.png') });
		fs.writeFileSync(path.join(out, name + '.texts.txt'),
			seen.texts.map(t => [...t.box, t.fill, t.text].join('\t') + '\n').join(''));
	} finally {
		await page.close();
	}
	return problems.map(p => `mockups: ${name}: ${p}`);
}

function planted(style, text) {
	return `<svg xmlns="http://www.w3.org/2000/svg" width="640" height="480">
  <defs><style>
${style}
  </style></defs>
  <rect width="640" height="480" fill="#0c0c34"/>
  <text x="30" y="42" font-size="16" fill="#f8f8f8">Generate</text>
  <text x="30" y="114" font-size="12" fill="#f8f8f8">${text}</text>
</svg>
`;
}

const FACE = "@font-face { font-family: 'Press Start 2P'; src: url('../../data/PressStart2P.ttf') format('truetype'); }";
const MISSING_FACE = FACE.replace('PressStart2P.ttf', 'Missing.ttf');
const IMPORT = "@import url('https://fonts.googleapis.com/css2?family=Press+Start+2P&amp;display=swap');";
const RULE = "text { font-family: 'Press Start 2P', monospace; font-variant-ligatures: none; }";
const LIGATURES_RULE = "text { font-family: 'Press Start 2P', monospace; }";
const IMPORT_URL = 'https://fonts.googleapis.com/css2?family=Press+Start+2P&display=swap';

const CASES = [
	['clean', planted(`${FACE}\n${RULE}`, 'Enemy Shuffle'), []],
	['external', planted(`${IMPORT}\n${FACE}\n${RULE}`, 'Enemy Shuffle'),
		[`mockups: external: external request ${IMPORT_URL}`]],
	['missing', planted(`${MISSING_FACE}\n${RULE}`, 'Enemy Shuffle'),
		[`mockups: missing: font ${FAMILY} not loaded`]],
	['ligature', planted(`${FACE}\n${LIGATURES_RULE}`, 'Enemy Shuffle'), null],
];

async function selfTest(browser) {
	const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'mockup-render-'));
	const failures = [];
	try {
		fs.mkdirSync(path.join(dir, 'data'));
		fs.copyFileSync(FONT, path.join(dir, 'data', 'PressStart2P.ttf'));
		const svgs = path.join(dir, 'design', 'mockups');
		fs.mkdirSync(svgs, { recursive: true });
		for (const [name, svg, want] of CASES) {
			const file = path.join(svgs, name + '.svg');
			fs.writeFileSync(file, svg);
			const got = await check(browser, file, path.join(dir, 'out'));
			const ok = want ? JSON.stringify(got) === JSON.stringify(want)
				: got.length === 1 && got[0].startsWith(`mockups: ${name}: text "Enemy Shuffle" is `);
			if (!ok)
				failures.push(`mockups: self-test: ${name}: want ${JSON.stringify(want || 'one text line')}, got ${JSON.stringify(got)}`);
		}
		const texts = fs.readFileSync(path.join(dir, 'out', 'clean.texts.txt'), 'utf8').split('\n').filter(Boolean)
			.map(l => l.split('\t')).map(f => [f[0], Math.round(f[2]), f[4], f[5]].join(' '));
		const wantTexts = ['30 128 f8f8f8 Generate', '30 156 f8f8f8 Enemy Shuffle'];
		if (JSON.stringify(texts) !== JSON.stringify(wantTexts))
			failures.push(`mockups: self-test: clean texts: want ${JSON.stringify(wantTexts)}, got ${JSON.stringify(texts)}`);
	} finally {
		fs.rmSync(dir, { recursive: true, force: true });
	}
	return failures;
}

async function main(argv) {
	if (argv.length < 2) {
		console.log('usage: node mockup_render.js OUT SVG...');
		return 2;
	}
	const [out, ...svgs] = argv;
	let browser;
	try {
		browser = await chromium.launch();
	} catch (e) {
		console.log(`setup: headless Chromium does not launch (${e.message.split('\n')[0]})`);
		return 1;
	}
	try {
		const failures = await selfTest(browser);
		if (failures.length) {
			failures.forEach(l => console.log(l));
			return 1;
		}
		console.log(`mockups: render check proved on ${CASES.length} planted SVGs`);
		const lines = [];
		for (const svg of svgs)
			lines.push(...await check(browser, svg, out));
		lines.forEach(l => console.log(l));
		if (lines.length)
			return 1;
		console.log(`mockups: ${svgs.length} mockups render offline with ${FAMILY}, every text at its width, into ${out}`);
		return 0;
	} finally {
		await browser.close();
	}
}

main(process.argv.slice(2)).then(code => process.exit(code), e => {
	console.log(`mockups: ${e.stack || e}`);
	process.exit(1);
});
