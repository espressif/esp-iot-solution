/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD */
/* SPDX-License-Identifier: Apache-2.0 */
import * as std from 'std';
import * as os from 'os';

Promise.resolve().then(() => {
    if (os.platform !== 'esp') {
        throw new Error(`unexpected platform: ${os.platform}`);
    }

    const [cwd, err] = os.getcwd();
    if (err !== 0) {
        throw new Error(`os.getcwd failed: ${std.strerror(err)}`);
    }

    const [scriptPath, realpathErr] = os.realpath('.');

    print(`hello from ${os.platform}: cwd=${cwd}, realpath=${realpathErr === 0 ? scriptPath : 'unavailable'}`);
});
