/**
 * @license
 * Copyright 2026 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import {esbuildPlugin} from '@web/dev-server-esbuild';
import {jasmineTestRunnerConfig} from 'web-test-runner-jasmine';

export default {
  // The following would keep web-test-runner-jasmine's testRunnerHtml as is:
  // ...jasmineTestRunnerConfig(),
  // Instead, reuse most of that testRunnerHtml and inject codec_wasm_bin.js.
  reporters: jasmineTestRunnerConfig().reporters,
  testRunnerHtml:
      (testRunnerImport, config) => {
        const html =
            jasmineTestRunnerConfig().testRunnerHtml(testRunnerImport, config);
        const parts = html.split('<head>');
        return parts[0].concat(
            '<head>',
            '<script type="text/javascript" src="codec_wasm_bin.js"></script>',
            parts[1]);
      },

  nodeResolve: true,
  files: ['*_test.ts'],
  plugins: [esbuildPlugin({ts: true, tsconfig: './tsconfig.json'})],
};
