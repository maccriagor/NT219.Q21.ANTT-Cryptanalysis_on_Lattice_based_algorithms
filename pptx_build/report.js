const pptxgen = require("pptxgenjs");
const pres = new pptxgen();
pres.layout = "LAYOUT_WIDE"; // 13.3 x 7.5
pres.author = "NT219";
pres.title = "PQC Implementation & Benchmark";

// ---- palette (cyber / post-quantum) ----
const NAVY = "0F1B3D", NAVY2 = "0A1230", CYAN = "06B6D4", CYAN_L = "67E8F9";
const WHITE = "FFFFFF", INK = "0F172A", BODY_C = "334155", MUTE = "64748B";
const TEAL = "0D9488", GREEN = "16A34A", RED = "DC2626", AMBER = "D97706";
const CARD = "F1F5F9", CARD2 = "E8EEF6", LINE = "CBD5E1";
const HEAD = "Trebuchet MS", BODY = "Calibri", MONO = "Consolas";
const sh = () => ({ type: "outer", color: "0F172A", blur: 7, offset: 3, angle: 135, opacity: 0.12 });
const W = 13.33;

function header(slide, kicker, title) {
  slide.background = { color: WHITE };
  slide.addShape(pres.shapes.RECTANGLE, { x: 0.6, y: 0.56, w: 0.17, h: 0.17, fill: { color: CYAN } });
  slide.addText(kicker.toUpperCase(), { x: 0.88, y: 0.5, w: 11.8, h: 0.3, fontFace: BODY, fontSize: 11.5, color: TEAL, bold: true, charSpacing: 2, margin: 0 });
  slide.addText(title, { x: 0.6, y: 0.8, w: 12.1, h: 0.72, fontFace: HEAD, fontSize: 27, color: INK, bold: true, margin: 0 });
}
function card(slide, x, y, w, h, fill) {
  slide.addShape(pres.shapes.ROUNDED_RECTANGLE, { x, y, w, h, rectRadius: 0.08, fill: { color: fill || WHITE }, line: { color: LINE, width: 1 }, shadow: sh() });
}
function chipColor(s) { return s.includes("✅") ? GREEN : s.includes("🟡") ? AMBER : MUTE; }

// ============================ Slide 1 — TITLE ============================
let s = pres.addSlide();
s.background = { color: NAVY };
s.addShape(pres.shapes.RECTANGLE, { x: 0, y: 0, w: W, h: 0.16, fill: { color: CYAN } });
s.addText("NT219 — CRYPTOGRAPHY & APPLICATIONS · ĐỒ ÁN", { x: 0.9, y: 1.1, w: 11.5, h: 0.4, fontFace: BODY, fontSize: 13, color: CYAN_L, bold: true, charSpacing: 2 });
s.addText("Triển khai & Đo hiệu năng\nMật mã Hậu lượng tử", { x: 0.85, y: 1.55, w: 11.6, h: 1.9, fontFace: HEAD, fontSize: 40, color: WHITE, bold: true, lineSpacingMultiple: 1.02 });
s.addText("ML-KEM (FIPS 203) · ML-DSA (FIPS 204)  vs  RSA / ECC — TLS 1.3 PQC trên x86 & ARM", { x: 0.9, y: 3.7, w: 11.6, h: 0.5, fontFace: BODY, fontSize: 17, color: "CBD5E1" });
[["OpenSSL 3.6.1 native", CYAN], ["NIST Cat 1/3/5", TEAL], ["KAT 9/9 PASS", GREEN], ["X25519MLKEM768 + ML-DSA-65", CYAN]].forEach((c, i) => {
  const x = 0.9 + i * 2.95;
  s.addShape(pres.shapes.ROUNDED_RECTANGLE, { x, y: 5.1, w: 2.8, h: 0.62, rectRadius: 0.31, fill: { color: NAVY2 }, line: { color: c[1], width: 1.2 } });
  s.addText(c[0], { x, y: 5.1, w: 2.8, h: 0.62, fontFace: MONO, fontSize: 11.5, color: c[1], bold: true, align: "center", valign: "middle" });
});
s.addText("Số x86 = ĐO THẬT (i5-12450HX) · Số ARM/năng lượng/netem = MÔ HÌNH (ghi rõ)", { x: 0.9, y: 6.45, w: 11.6, h: 0.4, fontFace: BODY, fontSize: 12, italic: true, color: "94A3B8" });

// ============================ Slide 2 — REQUIREMENTS ============================
s = pres.addSlide();
header(s, "Đồ án yêu cầu gì", "Hai nhóm yêu cầu: kỹ thuật (đề tài) + hệ thống (rubric)");
function reqCard(x, titleTxt, color, items) {
  card(s, x, 1.7, 5.85, 5.1, WHITE);
  s.addShape(pres.shapes.RECTANGLE, { x: x, y: 1.7, w: 0.12, h: 5.1, fill: { color: color } });
  s.addText(titleTxt, { x: x + 0.35, y: 1.95, w: 5.3, h: 0.5, fontFace: HEAD, fontSize: 17, bold: true, color: INK, margin: 0 });
  s.addText(items.map((t, i) => ({ text: t, options: { bullet: { code: "2022", indent: 14 }, breakLine: true, paraSpaceAfter: 7, color: BODY_C } })),
    { x: x + 0.35, y: 2.55, w: 5.2, h: 4.05, fontFace: BODY, fontSize: 13.5, margin: 0 });
}
reqCard(0.6, "① Kỹ thuật — đề tài (file 05)", CYAN, [
  "Implement + tối ưu + ĐO ML-KEM (KEM) & ML-DSA (chữ ký)",
  "So sánh RSA / ECDSA / ECDH ở mức bảo mật khớp Cat 1/3/5",
  "Hai nền tảng: x86_64 và ARM (Cortex-A72)",
  "Microbench: keygen / encaps / decaps / sign / verify",
  "Macrobench: TLS 1.3 handshake + bộ nhớ / kích thước / năng lượng",
  "Trả lời 3 câu hỏi nghiên cứu RQ1 / RQ2 / RQ3",
]);
reqCard(6.88, "② Hệ thống — rubric (NT2205)", TEAL, [
  "Asset-centric: danh mục tài sản A1–A5 + ràng buộc",
  "Giải pháp mật mã 3 lớp onion: Cryptography / AuthN / AuthZ",
  "≥ 2 kịch bản triển khai độc lập (x86 + ARM)",
  "Threat model + mục tiêu bảo vệ định lượng (SMART G1–G13)",
  "Invariants I1–I7 + đánh giá định lượng (E-Crypto/N/Z)",
  "Không cố định thuật toán lỗi thời (PQC native, bỏ OQS-fork)",
]);

// ============================ Slide 3 — WHAT WAS DONE (WP table) ============================
s = pres.addSlide();
header(s, "Đã làm gì", "Bao phủ Work Packages — sản phẩm cụ thể từng phần");
const wp = [
  ["WP0", "Lý thuyết · lit review · threat model · SMART · asset A1–A5", "✅ xong"],
  ["WP1", "Môi trường Docker + OpenSSL 3.6.1 native (vendored, x86 + ARM)", "✅ xong"],
  ["WP2", "Hàm đo tự viết (OpenSSL EVP) · microbench x86 · KAT 9/9 PASS", "✅ xong"],
  ["WP3", "AVX2 vs reference-C (đo thật) · NEON trên ARM (mô hình)", "🟡 một phần"],
  ["WP4", "TLS 1.3 PQC handshake THẬT: X25519MLKEM768 + cert ML-DSA-65", "✅ xong"],
  ["WP5", "Kiến trúc onion 3 lớp + ánh xạ PQC + invariants", "✅ xong"],
  ["WP6", "Code size · peak RSS (đo thật) · năng lượng (mô hình)", "🟡 một phần"],
  ["WP7", "Thống kê · 8 biểu đồ · trả lời RQ1/2/3 · bộ tài liệu", "✅ xong"],
];
const rows = [[
  { text: "WP", options: { fill: { color: NAVY }, color: WHITE, bold: true, align: "center", fontFace: HEAD } },
  { text: "Sản phẩm cụ thể", options: { fill: { color: NAVY }, color: WHITE, bold: true, fontFace: HEAD } },
  { text: "Trạng thái", options: { fill: { color: NAVY }, color: WHITE, bold: true, align: "center", fontFace: HEAD } },
]];
wp.forEach((r, i) => {
  const bg = i % 2 ? CARD : WHITE;
  rows.push([
    { text: r[0], options: { fill: { color: bg }, bold: true, color: INK, align: "center", fontFace: MONO } },
    { text: r[1], options: { fill: { color: bg }, color: BODY_C } },
    { text: r[2], options: { fill: { color: bg }, bold: true, color: chipColor(r[2]), align: "center" } },
  ]);
});
s.addTable(rows, { x: 0.6, y: 1.75, w: 12.1, colW: [1.0, 9.1, 2.0], rowH: 0.56, fontFace: BODY, fontSize: 13.5, valign: "middle", border: { type: "solid", pt: 0.5, color: LINE } });
s.addText("→ 6/8 WP hoàn tất; 2 WP một phần (NEON & năng lượng là MÔ HÌNH — chờ chạy thật trên Pi).", { x: 0.6, y: 6.85, w: 12, h: 0.4, fontFace: BODY, fontSize: 12.5, italic: true, color: MUTE });

// ============================ Slide 4 — METHOD & ENV ============================
s = pres.addSlide();
header(s, "Phương pháp & môi trường", "Đo đúng đắn, tái lập, và liêm chính dữ liệu");
const mc = [
  ["Môi trường (WP1)", CYAN, ["OpenSSL 3.6.1 native — ML-KEM/ML-DSA built-in", "KHÔNG dùng OQS-OpenSSL fork (đã archived)", "Docker + nguồn vendored (mạng hạn chế)", "PQClean reference-C cho bản đối chứng"]],
  ["Hàm đo tự viết (WP2)", TEAL, ["OpenSSL EVP API (Q_keygen, encapsulate, DigestSign)", "Cấu hình đa mức 1/3/5; cổng ĐÚNG ĐẮN trước khi đo", "100 mẫu/mức · median + p95 + CI95", "rdtsc cycles + CLOCK_MONOTONIC_RAW · ghim core 0"]],
  ["Hai nền tảng", GREEN, ["x86: Intel i5-12450HX, AVX2 — ĐO THẬT", "ARM: Cortex-A72 (Pi 4), NEON — MÔ HÌNH", "So theo mức bảo mật NIST khớp nhau", "KAT 9/9 PASS (tin số đo)"]],
  ["Liêm chính dữ liệu", AMBER, ["x86 = MEASURED (đo trên phần cứng)", "ARM / năng lượng / netem = MODELED", "Số mô hình = số thật × hệ số có trích dẫn", "Mọi file mô hình có dòng # MODELED"]],
];
mc.forEach((c, i) => {
  const x = 0.6 + (i % 2) * 6.28, y = 1.7 + Math.floor(i / 2) * 2.62;
  card(s, x, y, 5.95, 2.42, WHITE);
  s.addShape(pres.shapes.OVAL, { x: x + 0.3, y: y + 0.28, w: 0.34, h: 0.34, fill: { color: c[1] } });
  s.addText(c[0], { x: x + 0.78, y: y + 0.22, w: 5.0, h: 0.45, fontFace: HEAD, fontSize: 15.5, bold: true, color: INK, margin: 0, valign: "middle" });
  s.addText(c[2].map(t => ({ text: t, options: { bullet: { code: "2022", indent: 12 }, breakLine: true, paraSpaceAfter: 3, color: BODY_C } })),
    { x: x + 0.32, y: y + 0.78, w: 5.5, h: 1.55, fontFace: BODY, fontSize: 11.8, margin: 0 });
});

// ============================ Slide 5 — ARCHITECTURE (onion) ============================
s = pres.addSlide();
header(s, "Kiến trúc giải pháp", "Onion 3 lớp trong — PQC ở lõi Cryptography");
// nested rounded rectangles (onion)
const cx = 4.0, cy = 4.35;
const rings = [
  [5.6, 4.6, CARD2, "① Authentication — cert ML-DSA-65 (xác thực server / mTLS)", INK],
  [4.3, 3.5, "DBEAFE", "② Authorization — token ký ML-DSA (RBAC → ABAC)", INK],
  [3.0, 2.4, "A5F3FC", "③ Cryptography — ML-KEM (KEX hybrid) + AES-256-GCM", INK],
];
rings.forEach(r => {
  s.addShape(pres.shapes.ROUNDED_RECTANGLE, { x: cx - r[0] / 2, y: cy - r[1] / 2, w: r[0], h: r[1], rectRadius: 0.12, fill: { color: r[2] }, line: { color: WHITE, width: 2 } });
  s.addText(r[3], { x: cx - r[0] / 2 + 0.1, y: cy - r[1] / 2 + 0.12, w: r[0] - 0.2, h: 0.4, fontFace: BODY, fontSize: 10.5, bold: true, color: r[4], align: "center", margin: 0 });
});
s.addShape(pres.shapes.OVAL, { x: cx - 0.85, y: cy - 0.55, w: 1.7, h: 1.1, fill: { color: NAVY } });
s.addText("ASSET", { x: cx - 0.85, y: cy - 0.55, w: 1.7, h: 1.1, fontFace: HEAD, fontSize: 15, bold: true, color: CYAN_L, align: "center", valign: "middle" });
s.addText("Client  ──TLS 1.3 hybrid──▶", { x: cx - 2.6, y: 6.55, w: 5.2, h: 0.4, fontFace: MONO, fontSize: 11, color: MUTE, align: "center" });
// right column explanation
card(s, 7.7, 1.75, 5.0, 4.95, WHITE);
s.addText("Ánh xạ PQC vào 3 lớp", { x: 8.0, y: 2.0, w: 4.5, h: 0.45, fontFace: HEAD, fontSize: 16, bold: true, color: INK, margin: 0 });
[["Cryptography", "ML-KEM trao khóa (hybrid X25519MLKEM768) · AES-256-GCM bảo vệ dữ liệu (AEAD)", TEAL],
 ["Authentication", "Chứng chỉ server ký ML-DSA-65 · mTLS dịch vụ (CA ML-DSA)", CYAN],
 ["Authorization", "Token (JWT) ký ML-DSA · pin alg · PoP-bound · deny-by-default", "7C3AED"]].forEach((r, i) => {
  const y = 2.6 + i * 1.32;
  s.addShape(pres.shapes.RECTANGLE, { x: 8.0, y: y, w: 0.1, h: 1.1, fill: { color: r[2] } });
  s.addText(r[0], { x: 8.22, y: y - 0.02, w: 4.3, h: 0.4, fontFace: HEAD, fontSize: 13.5, bold: true, color: r[2], margin: 0 });
  s.addText(r[1], { x: 8.22, y: y + 0.38, w: 4.3, h: 0.8, fontFace: BODY, fontSize: 11.5, color: BODY_C, margin: 0 });
});

// ============================ Slide 6 — THREAT MODEL + SMART ============================
s = pres.addSlide();
header(s, "Rủi ro bảo mật & mục tiêu", "Threat model → giảm thiểu → mục tiêu định lượng (SMART)");
card(s, 0.6, 1.7, 7.1, 5.1, WHITE);
s.addText("Đe dọa chính → Giảm thiểu", { x: 0.95, y: 1.95, w: 6.5, h: 0.45, fontFace: HEAD, fontSize: 16, bold: true, color: INK, margin: 0 });
const threats = [
  ["Harvest-now, decrypt-later (HNDL)", "KEX hybrid X25519MLKEM768 — an toàn nếu 1 nhánh còn vững"],
  ["Shor phá RSA/ECC (lượng tử)", "ML-KEM + ML-DSA (Module-LWE / SIS)"],
  ["Grover giảm ½ đối xứng", "Category 5 ⇒ AES-256-GCM"],
  ["Downgrade / cấu hình sai", "Chỉ TLS 1.3 + nhóm hybrid/x25519 · pin alg"],
  ["Tamper ciphertext/chữ ký", "AEAD tag + ML-DSA verify → từ chối + log"],
];
threats.forEach((t, i) => {
  const y = 2.55 + i * 0.85;
  s.addShape(pres.shapes.RECTANGLE, { x: 0.95, y: y, w: 0.09, h: 0.68, fill: { color: RED } });
  s.addText(t[0], { x: 1.15, y: y - 0.02, w: 6.3, h: 0.32, fontFace: BODY, fontSize: 12.5, bold: true, color: INK, margin: 0 });
  s.addText("→ " + t[1], { x: 1.15, y: y + 0.3, w: 6.3, h: 0.38, fontFace: BODY, fontSize: 11.5, color: TEAL, margin: 0 });
});
card(s, 7.9, 1.7, 4.8, 5.1, NAVY);
s.addText("Mục tiêu SMART đã ĐẠT (đo thật)", { x: 8.2, y: 1.95, w: 4.3, h: 0.45, fontFace: HEAD, fontSize: 15.5, bold: true, color: CYAN_L, margin: 0 });
[["G3", "Toàn vẹn: KAT 9/9 PASS · tamper-reject 100%"],
 ["G5", "Handshake hậu lượng tử: nhóm X25519MLKEM768 + cert mldsa65"],
 ["G13", "Overhead handshake hybrid ≈ 9% (≤ 10%) — server chấp nhận được"],
 ["I7", "Kháng lượng tử / HNDL: KEX hybrid hoạt động thật"]].forEach((g, i) => {
  const y = 2.6 + i * 1.0;
  s.addShape(pres.shapes.ROUNDED_RECTANGLE, { x: 8.2, y: y, w: 0.7, h: 0.5, rectRadius: 0.1, fill: { color: CYAN } });
  s.addText(g[0], { x: 8.2, y: y, w: 0.7, h: 0.5, fontFace: HEAD, fontSize: 14, bold: true, color: NAVY, align: "center", valign: "middle" });
  s.addText(g[1], { x: 9.05, y: y - 0.05, w: 3.55, h: 0.95, fontFace: BODY, fontSize: 11.5, color: "E2E8F0", valign: "middle", margin: 0 });
});

// ============================ Slide 7 — MICROBENCH PQC (real x86) ============================
s = pres.addSlide();
header(s, "Kết quả · WP2 (x86 ĐO THẬT)", "Microbenchmark PQC reference-C — latency theo mức NIST");
s.addChart(pres.charts.BAR, [
  { name: "Keygen", labels: ["ML-KEM-512", "ML-KEM-768", "ML-KEM-1024"], values: [34, 58, 82] },
  { name: "Encaps", labels: ["ML-KEM-512", "ML-KEM-768", "ML-KEM-1024"], values: [36, 62, 83] },
  { name: "Decaps", labels: ["ML-KEM-512", "ML-KEM-768", "ML-KEM-1024"], values: [42, 72, 94] },
], { x: 0.5, y: 1.75, w: 6.0, h: 3.7, barDir: "col", chartColors: [CYAN, TEAL, "0E7490"], showTitle: true, title: "ML-KEM (µs, median)", titleFontFace: HEAD, titleFontSize: 13, showValue: true, dataLabelFontSize: 8, dataLabelColor: INK, dataLabelPosition: "outEnd", showLegend: true, legendPos: "b", legendFontSize: 10, catAxisLabelColor: MUTE, catAxisLabelFontSize: 9, valAxisLabelColor: MUTE, valGridLine: { color: "EEF2F7" } });
s.addChart(pres.charts.BAR, [
  { name: "Keygen", labels: ["ML-DSA-44", "ML-DSA-65", "ML-DSA-87"], values: [90, 168, 267] },
  { name: "Sign (median)", labels: ["ML-DSA-44", "ML-DSA-65", "ML-DSA-87"], values: [315, 454, 563] },
  { name: "Verify", labels: ["ML-DSA-44", "ML-DSA-65", "ML-DSA-87"], values: [83, 139, 239] },
], { x: 6.7, y: 1.75, w: 6.1, h: 3.7, barDir: "col", chartColors: ["7C3AED", "DC2626", "0D9488"], showTitle: true, title: "ML-DSA (µs, median)", titleFontFace: HEAD, titleFontSize: 13, showValue: true, dataLabelFontSize: 8, dataLabelColor: INK, dataLabelPosition: "outEnd", showLegend: true, legendPos: "b", legendFontSize: 10, catAxisLabelColor: MUTE, catAxisLabelFontSize: 9, valAxisLabelColor: MUTE, valGridLine: { color: "EEF2F7" } });
card(s, 0.6, 5.75, 12.1, 1.15, CARD);
s.addText([
  { text: "Validate FIPS 203/204: ", options: { bold: true, color: GREEN } },
  { text: "ML-KEM 512/768/1024 pk = 800/1184/1568 B, ct = 768/1088/1568 B · ML-DSA-87 pk = 2592 B, sig = 4627 B  —  khớp chính xác.   ", options: { color: BODY_C } },
  { text: "KAT 9/9 PASS", options: { bold: true, color: GREEN } },
  { text: " trước khi tin số đo.", options: { color: BODY_C } },
], { x: 0.9, y: 5.9, w: 11.5, h: 0.85, fontFace: BODY, fontSize: 13, valign: "middle", margin: 0 });

// ============================ Slide 8 — RQ1 optimization ============================
s = pres.addSlide();
header(s, "RQ1 · Yếu tố ảnh hưởng (x86 đo thật)", "Mức tối ưu SIMD quan trọng hơn mức tham số");
s.addChart(pres.charts.BAR, [
  { name: "reference-C", labels: ["Keygen", "Encaps", "Decaps"], values: [82, 83, 94] },
  { name: "AVX2 (OpenSSL native)", labels: ["Keygen", "Encaps", "Decaps"], values: [46, 22, 35] },
], { x: 0.6, y: 1.85, w: 6.7, h: 4.6, barDir: "col", chartColors: [MUTE, TEAL], showTitle: true, title: "ML-KEM-1024 — reference-C vs AVX2 (µs)", titleFontFace: HEAD, titleFontSize: 14, showValue: true, dataLabelFontSize: 11, dataLabelColor: INK, dataLabelPosition: "outEnd", showLegend: true, legendPos: "b", legendFontSize: 11, catAxisLabelColor: INK, valAxisLabelColor: MUTE, valGridLine: { color: "EEF2F7" } });
const sp = [["1.8×", "Keygen"], ["3.8×", "Encaps"], ["2.7×", "Decaps"]];
sp.forEach((c, i) => {
  const y = 2.0 + i * 1.5;
  card(s, 7.7, y, 5.0, 1.32, WHITE);
  s.addText(c[0], { x: 7.9, y: y + 0.12, w: 1.9, h: 1.05, fontFace: HEAD, fontSize: 38, bold: true, color: TEAL, align: "center", valign: "middle", margin: 0 });
  s.addText([{ text: c[1] + " nhanh hơn\n", options: { bold: true, color: INK } }, { text: "nhờ vector hoá AVX2", options: { color: MUTE } }], { x: 9.7, y: y + 0.12, w: 2.85, h: 1.05, fontFace: BODY, fontSize: 13, valign: "middle", margin: 0 });
});
s.addText("→ Tối ưu SIMD (AVX2 / NEON) cho 1.8–3.8× — yếu tố lớn nhất; tham số chỉ tăng đều theo mức.", { x: 0.6, y: 6.6, w: 12, h: 0.4, fontFace: BODY, fontSize: 13, italic: true, color: MUTE });

// ============================ Slide 9 — RQ1 PQC vs classical (trade-off) ============================
s = pres.addSlide();
header(s, "RQ1 · PQC vs cổ điển (Cat 5)", "Rẻ hơn nhiều về tính toán — đắt hơn nhiều về băng thông");
const stats = [
  ["22×", "nhanh hơn", "KEM: ML-KEM-1024 (57 µs) vs ECDH-P-521 (1284 µs)", TEAL],
  ["210×", "nhanh hơn", "Sign: ML-DSA-87 (563 µs) vs RSA-15360 (177 ms)", TEAL],
  ["78 000×", "nhanh hơn", "Keygen sig: ML-DSA-87 (267 µs) vs RSA-15360 (15.7 s!)", GREEN],
  ["33×", "LỚN hơn", "Chữ ký: ML-DSA-87 (4627 B) vs ECDSA-P-521 (139 B)", RED],
  ["24×", "LỚN hơn", "KEM ciphertext: ML-KEM-1024 (1568 B) vs ECDH (66 B)", RED],
];
stats.forEach((c, i) => {
  const x = 0.6 + (i % 3) * 4.06, y = 1.8 + Math.floor(i / 3) * 2.35;
  card(s, x, y, 3.85, 2.1, WHITE);
  s.addShape(pres.shapes.RECTANGLE, { x: x, y: y, w: 3.85, h: 0.1, fill: { color: c[3] } });
  s.addText(c[0], { x: x + 0.2, y: y + 0.25, w: 3.45, h: 0.85, fontFace: HEAD, fontSize: 40, bold: true, color: c[3], margin: 0 });
  s.addText(c[1], { x: x + 0.22, y: y + 1.08, w: 3.4, h: 0.35, fontFace: HEAD, fontSize: 15, bold: true, color: INK, margin: 0 });
  s.addText(c[2], { x: x + 0.22, y: y + 1.45, w: 3.45, h: 0.6, fontFace: BODY, fontSize: 11, color: MUTE, margin: 0 });
});
card(s, 8.72, 4.15, 4.0, 2.1, NAVY);
s.addText("Trade-off cốt lõi", { x: 8.95, y: 4.35, w: 3.6, h: 0.4, fontFace: HEAD, fontSize: 15, bold: true, color: CYAN_L, margin: 0 });
s.addText("PQC ở Cat 5 RẺ hơn nhiều về CPU (đặc biệt so RSA-15360 bất khả thi) nhưng ĐẮT hơn nhiều về băng thông (khóa/chữ ký lớn) — chi phí thật của migration.", { x: 8.95, y: 4.78, w: 3.6, h: 1.4, fontFace: BODY, fontSize: 12, color: "E2E8F0", margin: 0 });

// ============================ Slide 10 — RQ2 ARM/NEON (modeled) ============================
s = pres.addSlide();
header(s, "RQ2 · ARM Cortex-A72 + NEON", "NEON khả thi hoá PQC trên Raspberry Pi 4");
s.addShape(pres.shapes.ROUNDED_RECTANGLE, { x: 9.9, y: 0.55, w: 2.85, h: 0.5, rectRadius: 0.25, fill: { color: AMBER } });
s.addText("⚠ SỐ MÔ HÌNH", { x: 9.9, y: 0.55, w: 2.85, h: 0.5, fontFace: HEAD, fontSize: 12.5, bold: true, color: WHITE, align: "center", valign: "middle" });
s.addChart(pres.charts.BAR, [
  { name: "reference-C", labels: ["ML-KEM enc", "ML-KEM dec", "ML-DSA-87 verify", "ML-DSA-87 sign"], values: [382, 431, 1076, 2702] },
  { name: "+ NEON", labels: ["ML-KEM enc", "ML-KEM dec", "ML-DSA-87 verify", "ML-DSA-87 sign"], values: [182, 187, 598, 1589] },
], { x: 0.6, y: 1.85, w: 7.3, h: 4.6, barDir: "col", chartColors: [MUTE, CYAN], showTitle: true, title: "Cortex-A72 — ref-C vs NEON (µs, MÔ HÌNH)", titleFontFace: HEAD, titleFontSize: 13, showValue: true, dataLabelFontSize: 9, dataLabelColor: INK, dataLabelPosition: "outEnd", showLegend: true, legendPos: "b", legendFontSize: 11, catAxisLabelColor: INK, catAxisLabelFontSize: 9, valAxisLabelColor: MUTE, valGridLine: { color: "EEF2F7" } });
card(s, 8.2, 1.9, 4.5, 4.5, WHITE);
s.addText("Diễn giải", { x: 8.45, y: 2.1, w: 4.0, h: 0.4, fontFace: HEAD, fontSize: 15, bold: true, color: INK, margin: 0 });
s.addText([
  { text: "NEON kéo tốc độ ~1.7–2.3×", options: { bullet: { code: "2022" }, bold: true, color: TEAL, breakLine: true, paraSpaceAfter: 6 } },
  { text: "ML-KEM về ~0.18 ms/op, ML-DSA-87 verify ~0.6 ms", options: { bullet: { code: "2022" }, color: BODY_C, breakLine: true, paraSpaceAfter: 6 } },
  { text: "Đủ cho TLS server-class trên SBC (hàng trăm hs/giây)", options: { bullet: { code: "2022" }, color: BODY_C, breakLine: true, paraSpaceAfter: 6 } },
  { text: "AES không có ARMv8 crypto-ext (BCM2711) → ~22× chậm", options: { bullet: { code: "2022" }, color: RED, breakLine: true, paraSpaceAfter: 6 } },
  { text: "Mô hình: arm = x86ref × 4.6; cần chạy THẬT trên Pi để chốt", options: { bullet: { code: "2022" }, italic: true, color: AMBER } },
], { x: 8.45, y: 2.6, w: 4.05, h: 3.7, fontFace: BODY, fontSize: 12, margin: 0 });

// ============================ Slide 11 — RQ3 TLS handshake (real) ============================
s = pres.addSlide();
header(s, "RQ3 · TLS 1.3 PQC handshake (x86 ĐO THẬT)", "Overhead chấp nhận được — chi phí thật là kích thước cert");
s.addShape(pres.shapes.ROUNDED_RECTANGLE, { x: 0.6, y: 1.75, w: 7.2, h: 1.85, rectRadius: 0.08, fill: { color: NAVY } });
s.addText("OpenSSL 3.6.1 native · s_server ↔ s_client", { x: 0.9, y: 1.95, w: 6.6, h: 0.35, fontFace: BODY, fontSize: 11.5, color: CYAN_L, bold: true, margin: 0 });
s.addText([
  { text: "Negotiated TLS1.3 group: ", options: { color: "94A3B8", breakLine: false } },
  { text: "X25519MLKEM768\n", options: { color: CYAN_L, bold: true } },
  { text: "Peer signature type: ", options: { color: "94A3B8" } },
  { text: "mldsa65\n", options: { color: GREEN, bold: true } },
  { text: "Protocol: TLSv1.3   Cipher: TLS_AES_256_GCM_SHA384", options: { color: "E2E8F0" } },
], { x: 0.9, y: 2.4, w: 6.7, h: 1.1, fontFace: MONO, fontSize: 13.5, lineSpacingMultiple: 1.1, margin: 0 });
const tls = [
  ["Cấu hình", "Throughput", "Cert on-wire"],
  ["Cổ điển (ECDHE-P256 + ECDSA)", "2118 hs/s", "534 B"],
  ["PQC hybrid (X25519MLKEM768 + ML-DSA-65)", "1934 hs/s", "7464 B"],
  ["Chênh lệch", "≈ 9% overhead", "≈ 14× lớn hơn"],
];
const trows = tls.map((r, i) => r.map(cell => ({
  text: cell,
  options: i === 0 ? { fill: { color: NAVY }, color: WHITE, bold: true, fontFace: HEAD }
    : i === 3 ? { fill: { color: "FEF3C7" }, bold: true, color: AMBER }
    : { fill: { color: i % 2 ? CARD : WHITE }, color: BODY_C },
})));
s.addTable(trows, { x: 0.6, y: 3.95, w: 7.2, colW: [3.9, 1.75, 1.55], rowH: 0.6, fontFace: BODY, fontSize: 12, valign: "middle", border: { type: "solid", pt: 0.5, color: LINE } });
card(s, 8.05, 1.75, 4.65, 4.95, WHITE);
s.addText("Trả lời RQ3", { x: 8.35, y: 1.98, w: 4.1, h: 0.4, fontFace: HEAD, fontSize: 16, bold: true, color: INK, margin: 0 });
s.addText([
  { text: "Overhead CPU chỉ ~9% — KEM hậu lượng tử rẻ", options: { bullet: { code: "2022" }, color: BODY_C, breakLine: true, paraSpaceAfter: 9 } },
  { text: "Chi phí thật: cert ML-DSA lớn (~14×) → ~10.8 KB / 8 gói on-wire", options: { bullet: { code: "2022" }, color: INK, bold: true, breakLine: true, paraSpaceAfter: 9 } },
  { text: "Nhạy hơn dưới mất gói / MTU nhỏ (ma trận netem — MÔ HÌNH)", options: { bullet: { code: "2022" }, color: AMBER, breakLine: true, paraSpaceAfter: 9 } },
  { text: "Kết luận: hybrid PQC KHẢ DỤNG cho server (đạt G13 / I7)", options: { bullet: { code: "2022" }, color: TEAL, bold: true } },
], { x: 8.35, y: 2.5, w: 4.1, h: 4.0, fontFace: BODY, fontSize: 12.5, margin: 0 });

// ============================ Slide 12 — ML-DSA rejection sampling ============================
s = pres.addSlide();
header(s, "Phát hiện khoa học", "ML-DSA Sign lệch phải — rejection sampling (Fiat–Shamir-with-aborts)");
card(s, 0.6, 1.85, 6.0, 4.7, WHITE);
s.addText("ML-DSA-87 Sign (x86, đo thật)", { x: 0.95, y: 2.1, w: 5.4, h: 0.4, fontFace: HEAD, fontSize: 15, bold: true, color: INK, margin: 0 });
// skewed distribution sketch
const bx = 1.0, by = 5.55, bw = 5.3, bh = 2.4;
s.addShape(pres.shapes.LINE, { x: bx, y: by, w: bw, h: 0, line: { color: MUTE, width: 1 } });
const dist = [6, 18, 30, 26, 17, 11, 7, 4, 3, 2, 1.5, 1, 0.8];
const dmax = 30;
dist.forEach((v, i) => {
  const w = bw / dist.length, x = bx + i * w, hh = v / dmax * bh;
  s.addShape(pres.shapes.RECTANGLE, { x: x, y: by - hh, w: w * 0.85, h: hh, fill: { color: i < 4 ? CYAN : "BAE6FD" } });
});
s.addShape(pres.shapes.LINE, { x: bx + 1.4, y: by - bh, w: 0, h: bh, line: { color: GREEN, width: 1.5, dashType: "dash" } });
s.addText("median 563 µs", { x: bx + 0.7, y: by - bh - 0.28, w: 1.7, h: 0.25, fontFace: BODY, fontSize: 9.5, color: GREEN, bold: true, align: "center" });
s.addShape(pres.shapes.LINE, { x: bx + 3.7, y: by - bh, w: 0, h: bh, line: { color: RED, width: 1.5, dashType: "dash" } });
s.addText("p95 ~1.53 ms", { x: bx + 3.0, y: by - bh - 0.28, w: 1.7, h: 0.25, fontFace: BODY, fontSize: 9.5, color: RED, bold: true, align: "center" });
s.addText("latency mỗi lần ký (µs) →", { x: bx, y: by + 0.08, w: bw, h: 0.3, fontFace: BODY, fontSize: 10, color: MUTE, align: "center" });
card(s, 6.9, 1.85, 5.8, 4.7, CARD);
s.addText("Ý nghĩa đo lường", { x: 7.2, y: 2.1, w: 5.2, h: 0.4, fontFace: HEAD, fontSize: 16, bold: true, color: INK, margin: 0 });
s.addText([
  { text: "Mỗi lần ký lặp đến khi mẫu hợp lệ ⇒ thời gian biến thiên mạnh, lệch phải.", options: { color: BODY_C, breakLine: true, paraSpaceAfter: 10 } },
  { text: "median 563 µs · p95 ~1.53 ms · max ~2.25 ms", options: { fontFace: MONO, color: INK, bold: true, breakLine: true, paraSpaceAfter: 10 } },
  { text: "→ Bắt buộc báo cáo median + p95, KHÔNG dùng mean (sai lệch).", options: { color: RED, bold: true, breakLine: true, paraSpaceAfter: 10 } },
  { text: "Khớp đặc tính trong paper CRYSTALS-Dilithium ⇒ phương pháp đo đúng.", options: { color: TEAL, italic: true } },
], { x: 7.2, y: 2.65, w: 5.2, h: 3.6, fontFace: BODY, fontSize: 13.5, margin: 0 });

// ============================ Slide 13 — DONE vs REQUIRED + integrity ============================
s = pres.addSlide();
header(s, "Đã làm vs Yêu cầu", "Đối chiếu + invariants + liêm chính dữ liệu");
const inv = [
  ["I1 plaintext leak = 0", "E-C1", "✅ ĐO THẬT"],
  ["I2 tamper rejected", "E-C3 · KAT 9/9", "✅ ĐO THẬT"],
  ["I3 data-origin", "E-C3", "✅ ĐO THẬT"],
  ["I4 AuthN + PoP", "E-N1 / E-Z2", "🟡 một phần"],
  ["I5 explainability", "E-Z1", "◻ kế hoạch"],
  ["I6 key ops (rotate)", "E-N2", "◻ kế hoạch"],
  ["I7 PQC-ready + perf", "E-N1 · RQ3", "✅ ĐO THẬT"],
];
const irows = [[
  { text: "Invariant", options: { fill: { color: NAVY }, color: WHITE, bold: true, fontFace: HEAD } },
  { text: "Bài đo", options: { fill: { color: NAVY }, color: WHITE, bold: true, fontFace: HEAD } },
  { text: "Trạng thái", options: { fill: { color: NAVY }, color: WHITE, bold: true, align: "center", fontFace: HEAD } },
]];
inv.forEach((r, i) => {
  const bg = i % 2 ? CARD : WHITE;
  irows.push([
    { text: r[0], options: { fill: { color: bg }, color: INK, bold: true } },
    { text: r[1], options: { fill: { color: bg }, color: BODY_C, fontFace: MONO, fontSize: 11 } },
    { text: r[2], options: { fill: { color: bg }, bold: true, color: r[2].includes("✅") ? GREEN : r[2].includes("🟡") ? AMBER : MUTE, align: "center" } },
  ]);
});
s.addTable(irows, { x: 0.6, y: 1.8, w: 6.3, colW: [3.0, 1.9, 1.4], rowH: 0.51, fontFace: BODY, fontSize: 12, valign: "middle", border: { type: "solid", pt: 0.5, color: LINE } });
card(s, 7.15, 1.8, 5.55, 4.95, WHITE);
s.addText("Liêm chính dữ liệu (khoa học)", { x: 7.45, y: 2.05, w: 5.0, h: 0.4, fontFace: HEAD, fontSize: 16, bold: true, color: INK, margin: 0 });
s.addShape(pres.shapes.ROUNDED_RECTANGLE, { x: 7.45, y: 2.65, w: 4.95, h: 1.5, rectRadius: 0.08, fill: { color: "ECFDF5" }, line: { color: GREEN, width: 1 } });
s.addText([{ text: "ĐO THẬT (x86, i5-12450HX)\n", options: { bold: true, color: GREEN } },
  { text: "PQC ref-C & AVX2 · cổ điển · KAT · TLS handshake · code size · RSS", options: { color: BODY_C } }], { x: 7.7, y: 2.8, w: 4.5, h: 1.2, fontFace: BODY, fontSize: 12, valign: "middle", margin: 0 });
s.addShape(pres.shapes.ROUNDED_RECTANGLE, { x: 7.45, y: 4.3, w: 4.95, h: 1.5, rectRadius: 0.08, fill: { color: "FFFBEB" }, line: { color: AMBER, width: 1 } });
s.addText([{ text: "MÔ HÌNH (ước lượng có trích dẫn)\n", options: { bold: true, color: AMBER } },
  { text: "ARM Cortex-A72 · NEON · năng lượng · ma trận netem — số thật × hệ số", options: { color: BODY_C } }], { x: 7.7, y: 4.45, w: 4.5, h: 1.2, fontFace: BODY, fontSize: 12, valign: "middle", margin: 0 });
s.addText("Không số mô hình nào bị trình bày như số đo (DATA_PROVENANCE.md).", { x: 7.45, y: 5.95, w: 5.0, h: 0.6, fontFace: BODY, fontSize: 11.5, italic: true, color: MUTE, margin: 0 });

// ============================ Slide 14 — CONCLUSION ============================
s = pres.addSlide();
s.background = { color: NAVY };
s.addShape(pres.shapes.RECTANGLE, { x: 0, y: 0, w: W, h: 0.16, fill: { color: CYAN } });
s.addShape(pres.shapes.RECTANGLE, { x: 0.85, y: 0.95, w: 0.17, h: 0.17, fill: { color: CYAN } });
s.addText("KẾT LUẬN", { x: 1.12, y: 0.88, w: 8, h: 0.35, fontFace: BODY, fontSize: 13, color: CYAN_L, bold: true, charSpacing: 2 });
s.addText("PQC khả dụng thực tế — rẻ về tính toán, đắt về băng thông", { x: 0.85, y: 1.25, w: 11.6, h: 0.8, fontFace: HEAD, fontSize: 26, bold: true, color: WHITE });
[["RQ1", "PQC ở Cat 5 rẻ hơn cổ điển (KEM 22×, sign 210× vs RSA-15360); yếu tố lớn nhất là tối ưu SIMD (AVX2/NEON 1.8–3.8×). Nhưng chữ ký/khóa lớn hơn 24–33×.", TEAL],
 ["RQ2", "NEON kéo PQC trên Cortex-A72 về ~0.18 ms (KEM) / ~0.6 ms (ML-DSA verify) → khả thi cho edge/SBC. (Số ARM là MÔ HÌNH — cần đo thật trên Pi.)", CYAN],
 ["RQ3", "Handshake hybrid TLS 1.3 overhead ~9% throughput — chấp nhận được; chi phí thật là cert PQC ~14×, đáng lưu ý khi mạng kém.", "67E8F9"]].forEach((r, i) => {
  const y = 2.35 + i * 1.15;
  s.addShape(pres.shapes.ROUNDED_RECTANGLE, { x: 0.85, y: y, w: 1.05, h: 0.6, rectRadius: 0.1, fill: { color: r[2] } });
  s.addText(r[0], { x: 0.85, y: y, w: 1.05, h: 0.6, fontFace: HEAD, fontSize: 15, bold: true, color: NAVY, align: "center", valign: "middle" });
  s.addText(r[1], { x: 2.1, y: y - 0.05, w: 10.4, h: 1.0, fontFace: BODY, fontSize: 13.5, color: "E2E8F0", valign: "middle", margin: 0 });
});
s.addShape(pres.shapes.LINE, { x: 0.85, y: 5.95, w: 11.6, h: 0, line: { color: "334155", width: 1 } });
s.addText([
  { text: "Đáp ứng đề tài 05 + rubric NT2205. ", options: { bold: true, color: WHITE } },
  { text: "Bước tiếp: chạy THẬT trên Pi (thay số ARM mô hình), đo năng lượng vật lý, build Apache PQC + netem 2-host.", options: { color: "94A3B8" } },
], { x: 0.85, y: 6.15, w: 11.6, h: 0.8, fontFace: BODY, fontSize: 13, margin: 0 });

pres.writeFile({ fileName: "../NT219_PQC_Report.pptx" }).then(f => console.log("WROTE:", f));
