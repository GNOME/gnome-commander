/*
 * Copyright 2025 Andrey Kutejko <andy128k@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * For more details see the file COPYING.
 */

// see "http://en.wikipedia.org/wiki/CP437"
pub const CP437: [char; 256] = [
    '\u{2E}', // NULL will be shown as a dot
    '\u{263A}', '\u{263B}', '\u{2665}', '\u{2666}', '\u{2663}', '\u{2660}', '\u{2022}', '\u{25D8}',
    '\u{25CB}', '\u{25D9}', '\u{2642}', '\u{2640}', '\u{266A}', '\u{266B}', '\u{263C}', '\u{25BA}',
    '\u{25C4}', '\u{2195}', '\u{203C}', '\u{B6}', '\u{A7}', '\u{25AC}', '\u{21A8}', '\u{2191}',
    '\u{2193}', '\u{2192}', '\u{2190}', '\u{221F}', '\u{2194}', '\u{25B2}', '\u{25BC}', '\u{20}',
    '\u{21}', '\u{22}', '\u{23}', '\u{24}', '\u{25}', '\u{26}', '\u{27}', '\u{28}', '\u{29}',
    '\u{2A}', '\u{2B}', '\u{2C}', '\u{2D}', '\u{2E}', '\u{2F}', '\u{30}', '\u{31}', '\u{32}',
    '\u{33}', '\u{34}', '\u{35}', '\u{36}', '\u{37}', '\u{38}', '\u{39}', '\u{3A}', '\u{3B}',
    '\u{3C}', '\u{3D}', '\u{3E}', '\u{3F}', '\u{40}', '\u{41}', '\u{42}', '\u{43}', '\u{44}',
    '\u{45}', '\u{46}', '\u{47}', '\u{48}', '\u{49}', '\u{4A}', '\u{4B}', '\u{4C}', '\u{4D}',
    '\u{4E}', '\u{4F}', '\u{50}', '\u{51}', '\u{52}', '\u{53}', '\u{54}', '\u{55}', '\u{56}',
    '\u{57}', '\u{58}', '\u{59}', '\u{5A}', '\u{5B}', '\u{5C}', '\u{5D}', '\u{5E}', '\u{5F}',
    '\u{60}', '\u{61}', '\u{62}', '\u{63}', '\u{64}', '\u{65}', '\u{66}', '\u{67}', '\u{68}',
    '\u{69}', '\u{6A}', '\u{6B}', '\u{6C}', '\u{6D}', '\u{6E}', '\u{6F}', '\u{70}', '\u{71}',
    '\u{72}', '\u{73}', '\u{74}', '\u{75}', '\u{76}', '\u{77}', '\u{78}', '\u{79}', '\u{7A}',
    '\u{7B}', '\u{7C}', '\u{7D}', '\u{7E}', '\u{2302}', '\u{C7}', '\u{FC}', '\u{E9}', '\u{E2}',
    '\u{E4}', '\u{E0}', '\u{E5}', '\u{E7}', '\u{EA}', '\u{EB}', '\u{E8}', '\u{EF}', '\u{EE}',
    '\u{EC}', '\u{C4}', '\u{C5}', '\u{C9}', '\u{E6}', '\u{C6}', '\u{F4}', '\u{F6}', '\u{F2}',
    '\u{FB}', '\u{F9}', '\u{FF}', '\u{D6}', '\u{DC}', '\u{A2}', '\u{A3}', '\u{A5}', '\u{20A7}',
    '\u{192}', '\u{E1}', '\u{ED}', '\u{F3}', '\u{FA}', '\u{F1}', '\u{D1}', '\u{AA}', '\u{BA}',
    '\u{BF}', '\u{2310}', '\u{AC}', '\u{BD}', '\u{BC}', '\u{A1}', '\u{AB}', '\u{BB}', '\u{2591}',
    '\u{2592}', '\u{2593}', '\u{2502}', '\u{2524}', '\u{2561}', '\u{2562}', '\u{2556}', '\u{2555}',
    '\u{2563}', '\u{2551}', '\u{2557}', '\u{255D}', '\u{255C}', '\u{255B}', '\u{2510}', '\u{2514}',
    '\u{2534}', '\u{252C}', '\u{251C}', '\u{2500}', '\u{253C}', '\u{255E}', '\u{255F}', '\u{255A}',
    '\u{2554}', '\u{2569}', '\u{2566}', '\u{2560}', '\u{2550}', '\u{256C}', '\u{2567}', '\u{2568}',
    '\u{2564}', '\u{2565}', '\u{2559}', '\u{2558}', '\u{2552}', '\u{2553}', '\u{256B}', '\u{256A}',
    '\u{2518}', '\u{250C}', '\u{2588}', '\u{2584}', '\u{258C}', '\u{2590}', '\u{2580}', '\u{3B1}',
    '\u{DF}', '\u{393}', '\u{3C0}', '\u{3A3}', '\u{3C3}', '\u{B5}', '\u{3C4}', '\u{3A6}',
    '\u{398}', '\u{3A9}', '\u{3B4}', '\u{221E}', '\u{3C6}', '\u{3B5}', '\u{2229}', '\u{2261}',
    '\u{B1}', '\u{2265}', '\u{2264}', '\u{2320}', '\u{2321}', '\u{F7}', '\u{2248}', '\u{B0}',
    '\u{2219}', '\u{B7}', '\u{221A}', '\u{207F}', '\u{B2}', '\u{25A0}', '\u{A0}',
];
