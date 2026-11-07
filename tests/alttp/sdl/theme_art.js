// Usage: NODE_PATH=$(npm root -g) node theme_art.js TABLE MOCKUPS OUT
// For each line of TABLE (theme_art.py's format), opens MOCKUPS/<mockup>.svg as file:// in headless
// Chromium, keeps only defs and the elements its selectors match (each exactly one), sets the
// root's viewBox to the clip box and its size to the box times the scale, and screenshots that size
// with a transparent background into OUT/<file>; a frame line's kept elements (and the defs they
// reference) must be the same markup in every MOCKUPS/*.svg. Writes OUT/chromium-version. Prints
// one "theme-art: <file>: <mockup>: <problem>" line per failure, exit 1 if any: a request that is
// not file: (aborted), no loaded "Press Start 2P" face. Proves itself first on planted mockups.
'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const zlib = require('zlib');

const FAMILY = 'Press Start 2P';
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

function readTable(file) {
	return fs.readFileSync(file, 'utf8').split('\n').filter(l => l.trim()).map(l => {
		const f = l.split('\t');
		const box = f.slice(3, 7).map(Number);
		const scale = Number(f[7]);
		if (f.length < 9 || !['frame', 'sprite'].includes(f[2]) || box.some(isNaN) || isNaN(scale))
			throw new Error(`table line ${JSON.stringify(l)}: want file, mockup, frame|sprite, x y w h, scale, selectors`);
		return { file: f[0], mockup: f[1], kind: f[2], box, scale, selectors: f.slice(8) };
	});
}

async function open(browser, svg, problems) {
	const page = await browser.newPage({ viewport: { width: 640, height: 480 } });
	await page.route('**/*', route => {
		const url = route.request().url();
		if (url.startsWith('file:'))
			return route.continue();
		problems.push(`external request ${url}`);
		return route.abort();
	});
	await page.goto('file://' + path.resolve(svg), { waitUntil: 'load' });
	const loaded = await page.evaluate(async family => {
		await document.fonts.ready;
		let ok = false;
		document.fonts.forEach(f => { ok = ok || (f.family.replace(/["']/g, '') === family && f.status === 'loaded'); });
		return ok;
	}, FAMILY);
	if (!loaded)
		problems.push(`font ${FAMILY} not loaded`);
	return page;
}

// Returns, per selector, its match count and the canonical markup of the match and of the defs it references.
function markup(page, selectors) {
	return page.evaluate(selectors => {
		const canon = el => {
			const attrs = [...el.attributes].map(a => `${a.name}=${JSON.stringify(a.value)}`).sort().join(' ');
			const kids = [...el.childNodes].map(n => n.nodeType === 1 ? canon(n)
				: n.nodeType === 3 && n.textContent.trim() ? JSON.stringify(n.textContent.trim()) : '').join('');
			return `<${el.tagName} ${attrs}>${kids}</${el.tagName}>`;
		};
		const refs = el => {
			const ids = new Set();
			for (const e of [el, ...el.querySelectorAll('*')])
				for (const a of e.attributes) {
					const m = a.value.match(/^#(.+)$/) || a.value.match(/url\(#([^)]+)\)/);
					if (m && (a.name === 'href' || a.name === 'xlink:href' || a.value.startsWith('url(')))
						ids.add(m[1]);
				}
			return [...ids].sort().map(id => document.getElementById(id)).map(d => d ? canon(d) : '<missing>').join('');
		};
		return selectors.map(s => {
			const found = document.querySelectorAll(s);
			return { count: found.length, markup: found.length === 1 ? canon(found[0]) + refs(found[0]) : null };
		});
	}, selectors);
}

async function render(browser, line, mockups, out) {
	const problems = [];
	const page = await open(browser, path.join(mockups, line.mockup + '.svg'), problems);
	const width = Math.round(line.box[2] * line.scale);
	const height = Math.round(line.box[3] * line.scale);
	try {
		const found = await markup(page, line.selectors);
		found.forEach((f, i) => {
			if (f.count !== 1)
				problems.push(`selector ${line.selectors[i]} matches ${f.count} elements, not 1`);
		});
		if (problems.length)
			return { problems, found };
		await page.evaluate(({ selectors, box, scale }) => {
			const root = document.documentElement;
			const kept = selectors.map(s => document.querySelector(s));
			const ancestors = new Set();
			for (const k of kept)
				for (let e = k; e && e !== root; e = e.parentElement)
					ancestors.add(e);
			const prune = parent => {
				for (const child of [...parent.children]) {
					if (child.tagName === 'defs' || kept.includes(child))
						continue;
					if (ancestors.has(child))
						prune(child);
					else
						child.remove();
				}
			};
			prune(root);
			root.setAttribute('viewBox', box.join(' '));
			root.setAttribute('width', box[2] * scale);
			root.setAttribute('height', box[3] * scale);
		}, line);
		fs.mkdirSync(out, { recursive: true });
		await page.screenshot({ path: path.join(out, line.file), clip: { x: 0, y: 0, width, height }, omitBackground: true });
		return { problems, found };
	} finally {
		await page.close();
	}
}

async function frameCheck(browser, line, mockups, found) {
	const problems = [];
	const svgs = fs.readdirSync(mockups).filter(n => n.endsWith('.svg')).sort();
	for (const svg of svgs) {
		const name = path.basename(svg, '.svg');
		if (name === line.mockup)
			continue;
		const own = [];
		const page = await open(browser, path.join(mockups, svg), own);
		try {
			const other = await markup(page, line.selectors);
			other.forEach((f, i) => {
				if (f.count !== 1)
					own.push(`selector ${line.selectors[i]} matches ${f.count} elements, not 1`);
				else if (f.markup !== found[i].markup)
					own.push(`selector ${line.selectors[i]} differs from ${line.mockup}`);
			});
		} finally {
			await page.close();
		}
		problems.push(...own.map(p => `theme-art: ${line.file}: ${name}: ${p}`));
	}
	return problems;
}

async function renderTable(browser, table, mockups, out) {
	const lines = [];
	for (const line of table) {
		const { problems, found } = await render(browser, line, mockups, out);
		lines.push(...problems.map(p => `theme-art: ${line.file}: ${line.mockup}: ${p}`));
		if (!problems.length && line.kind === 'frame')
			lines.push(...await frameCheck(browser, line, mockups, found));
	}
	return lines;
}

function decodePng(data) {
	let pos = 8;
	let header;
	const idat = [];
	while (pos + 8 <= data.length) {
		const length = data.readUInt32BE(pos);
		const kind = data.toString('latin1', pos + 4, pos + 8);
		const body = data.subarray(pos + 8, pos + 8 + length);
		pos += 12 + length;
		if (kind === 'IHDR')
			header = { width: body.readUInt32BE(0), height: body.readUInt32BE(4), depth: body[8], colour: body[9] };
		else if (kind === 'IDAT')
			idat.push(body);
	}
	if (!header || header.depth !== 8 || ![2, 6].includes(header.colour))
		throw new Error('not an 8-bit RGB or RGBA PNG');
	const { width, height } = header;
	const bpp = header.colour === 6 ? 4 : 3;
	const raw = zlib.inflateSync(Buffer.concat(idat));
	const stride = width * bpp;
	const rows = Buffer.alloc(stride * height);
	for (let y = 0; y < height; y++) {
		const kind = raw[y * (stride + 1)];
		for (let i = 0; i < stride; i++) {
			const x = raw[y * (stride + 1) + 1 + i];
			const a = i >= bpp ? rows[y * stride + i - bpp] : 0;
			const b = y ? rows[(y - 1) * stride + i] : 0;
			const c = i >= bpp && y ? rows[(y - 1) * stride + i - bpp] : 0;
			const p = a + b - c;
			const paeth = Math.abs(p - a) <= Math.abs(p - b) && Math.abs(p - a) <= Math.abs(p - c) ? a
				: Math.abs(p - b) <= Math.abs(p - c) ? b : c;
			rows[y * stride + i] = (x + [0, a, b, (a + b) >> 1, paeth][kind]) & 0xff;
		}
	}
	const pixels = Buffer.alloc(width * height * 4, 255);
	for (let j = 0; j < width * height; j++)
		rows.copy(pixels, j * 4, j * bpp, j * bpp + bpp);
	return { width, height, pixels };
}

function planted(body, head = '') {
	return `<svg xmlns="http://www.w3.org/2000/svg" width="640" height="480" viewBox="0 0 640 480" shape-rendering="crispEdges">
  <defs><style>
${head}
@font-face { font-family: 'Press Start 2P'; src: url('../../data/PressStart2P.ttf') format('truetype'); }
text { font-family: 'Press Start 2P', monospace; font-variant-ligatures: none; }
  </style></defs>
  <rect width="640" height="480" fill="#0000ff"/>
  <text x="30" y="42" font-size="16" fill="#f8f8f8">Generate</text>
${body}
</svg>
`;
}

const RED = '<rect x="4" y="2" width="8" height="4" fill="#ff0000"/>';
const RED_SELECTOR = 'rect[fill="#ff0000"]';
const EXTERNAL = 'http://example.invalid/planted.css';

function tableLine(file, mockup, kind, box, scale, selectors) {
	return { file, mockup, kind, box, scale, selectors };
}

function solid(width, height, rgba) {
	return Buffer.concat(Array(width * height).fill(Buffer.from(rgba)));
}

const CASES = [
	['an 8x4 red rect', { m: planted(RED) }, tableLine('red.png', 'm', 'sprite', [4, 2, 8, 4], 1, [RED_SELECTOR]),
		[], [8, 4, solid(8, 4, [255, 0, 0, 255])]],
	['the rect at scale 0.5', { m: planted(RED) }, tableLine('half.png', 'm', 'sprite', [4, 2, 8, 4], 0.5, [RED_SELECTOR]),
		[], [4, 2, solid(4, 2, [255, 0, 0, 255])]],
	['a selector matching nothing', { m: planted(RED) },
		tableLine('none.png', 'm', 'sprite', [4, 2, 8, 4], 1, ['rect[fill="#00ff00"]']),
		['theme-art: none.png: m: selector rect[fill="#00ff00"] matches 0 elements, not 1'], null],
	['a selector matching two', { m: planted(RED + RED.replace('x="4"', 'x="20"')) },
		tableLine('two.png', 'm', 'sprite', [4, 2, 8, 4], 1, [RED_SELECTOR]),
		[`theme-art: two.png: m: selector ${RED_SELECTOR} matches 2 elements, not 1`], null],
	['a frame differing in another mockup', { m: planted(RED), n: planted(RED.replace('width="8"', 'width="9"')) },
		tableLine('frame.png', 'm', 'frame', [0, 0, 16, 8], 1, [RED_SELECTOR]),
		[`theme-art: frame.png: n: selector ${RED_SELECTOR} differs from m`], null],
	['an http: request', { m: planted(RED, `@import url('${EXTERNAL}');`) },
		tableLine('external.png', 'm', 'sprite', [4, 2, 8, 4], 1, [RED_SELECTOR]),
		[`theme-art: external.png: m: external request ${EXTERNAL}`], null],
];

async function selfTest(browser) {
	const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'theme-art-'));
	const failures = [];
	try {
		fs.mkdirSync(path.join(dir, 'data'));
		fs.copyFileSync(FONT, path.join(dir, 'data', 'PressStart2P.ttf'));
		for (const [i, [name, svgs, line, want, image]] of CASES.entries()) {
			const mockups = path.join(dir, 'design', `case${i}`);
			fs.mkdirSync(mockups, { recursive: true });
			for (const [n, svg] of Object.entries(svgs))
				fs.writeFileSync(path.join(mockups, n + '.svg'), svg);
			const out = path.join(dir, 'out');
			const got = await renderTable(browser, [line], mockups, out);
			if (JSON.stringify(got) !== JSON.stringify(want))
				failures.push(`theme-art: self-test: ${name}: want ${JSON.stringify(want)}, got ${JSON.stringify(got)}`);
			if (!image)
				continue;
			const shot = decodePng(fs.readFileSync(path.join(out, line.file)));
			if (shot.width !== image[0] || shot.height !== image[1] || !shot.pixels.equals(image[2]))
				failures.push(`theme-art: self-test: ${name}: want ${image[0]}x${image[1]} opaque red, got ${shot.width}x${shot.height} ${shot.pixels.toString('hex').slice(0, 32)}`);
		}
	} finally {
		fs.rmSync(dir, { recursive: true, force: true });
	}
	return failures;
}

async function main(argv) {
	if (argv.length !== 3) {
		console.log('usage: node theme_art.js TABLE MOCKUPS OUT');
		return 2;
	}
	const [tableFile, mockups, out] = argv;
	const table = readTable(tableFile);
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
		console.log(`theme-art: renderer proved on ${CASES.length} planted mockups`);
		const lines = await renderTable(browser, table, mockups, out);
		lines.forEach(l => console.log(l));
		if (lines.length)
			return 1;
		fs.writeFileSync(path.join(out, 'chromium-version'), browser.version() + '\n');
		return 0;
	} finally {
		await browser.close();
	}
}

main(process.argv.slice(2)).then(code => process.exit(code), e => {
	console.log(`theme-art: ${e.stack || e}`);
	process.exit(1);
});
