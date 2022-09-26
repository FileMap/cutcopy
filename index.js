const {promisify} = require('util');
const {
  isCut,
  changeDropEffect,
  readFilePaths,
  writeFilePaths,
  clear,
  saveImageAsJpegSync,
  saveImageAsJpegAsync,
  saveImageAsPngSync,
  saveImageAsPngAsync,
  putImageSync,
  putImageAsync,
  hasImage,
} = require('node-gyp-build')(__dirname);

module.exports = {
  isCut,
  changeDropEffect,
  readFilePaths,
  writeFilePaths,
  clear,
  saveImageAsJpeg: promisify(saveImageAsJpegAsync),
  saveImageAsJpegSync,
  saveImageAsPng: promisify(saveImageAsPngAsync),
  saveImageAsPngSync,
  putImageSync,
  putImage: promisify(putImageAsync),
  hasImage,
};
