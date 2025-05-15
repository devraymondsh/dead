import puppeteer from "puppeteer-core";
import { readFileSync } from "node:fs";

const template = readFileSync("template.html", { encoding: "utf-8" });

const browser = await puppeteer.launch({
  executablePath: "/usr/bin/google-chrome-stable",
  args: ["--no-sandbox", "--disable-setuid-sandbox"],
});
const page = await browser.newPage();

const generatePDF = async (filename) => {
  // let html = template;
  // html = html.replace("{{__C_OR_CPP__}}", options.C_OR_CPP);
  // html = html.replace("{{__Rust_OR_Backend__}}", options.Rust_OR_Backend);

  await page.setContent(template, {
    waitUntil: "networkidle0",
  });
  await page.pdf({
    path: `/home/raymond/Documents/resume/${filename}`,
    format: "A4",
    margin: { top: 20, bottom: 20, left: 30, right: 30 },
  });
};

await generatePDF("MahdiSharifi_SoftwareEngineer.pdf");

// await generatePDF("Mahdi_RustEngineer.pdf", {
//   C_OR_CPP: "C++",
//   Rust_OR_Backend: "Rust",
// });
// await generatePDF("Mahdi_BackendEngineer.pdf", {
//   C_OR_CPP: "C++",
//   Rust_OR_Backend: "Backend",
// });
// await generatePDF("Mahdi_CppEngineer.pdf", {
//   C_OR_CPP: "C++",
//   Rust_OR_Backend: "Backend",
// });
// await generatePDF("Mahdi_SoftwareEngineer.pdf", {
//   C_OR_CPP: "C++",
//   Rust_OR_Backend: "Backend",
// });
// await generatePDF("Mahdi_ClangEngineer.pdf", {
//   C_OR_CPP: "C",
//   Rust_OR_Backend: "Backend",
// });

await page.close();
await browser.close();
