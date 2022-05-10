/*!
 * version 1.4.1 MIT
 * 2019-2-1 wenju
 * http://www.mescroll.com
 */

//const baseurl = 'http://192.168.4.1:80/'
const baseurl = window.location.origin
const CONSTANT = {
    NODES_INFO_URL: `${baseurl}v1/user/nodes/info`,
    NODES_PARAMS_URL: `${baseurl}v1/user/nodes/params`,
    HOST_IP: '192.168.4.1',
    ON_STATUS: 1,
    OFF_STATUS: 0,
    DEVICEINFO: 'esp_device_info',
    COMMANDINFO: 'command_info',
    COMMANDINDEX: 'command_index',
    DEVICELIST: 'device_list',
    G2P_DATA_LIST: 'g2p_data_list',
    PAGE_HOME: 'home',
    PAGE_OPERATE: 'operate',
    PAGE_ADD_DEVICE: 'addDevice',
    PAGE_ADD_DEVICE_INFO: 'addDeviceInfo',
    PAGE_COMMAND: 'command',
    DEVICE_TYPE_LIST: [
        {
            label: 'Lighting',
            name: 'Light',
            defaultIcon: 'light',
            disabled: false
        },
        {
            label: 'Switch',
            param: 'Switch',
            defaultIcon: 'switch',
            disabled: false
        },
        {
            label: 'Socket',
            param: 'Socket',
            defaultIcon: 'socket',
            disabled: true
        },
        {
            label: 'Fan',
            param: 'Fan',
            defaultIcon: 'fan',
            disabled: true
        },
        {
            label: 'Sensor',
            param: 'Sensor',
            defaultIcon: 'sensor',
            disabled: true
        },
        {
            label: 'Router',
            param: 'Router',
            defaultIcon: 'router',
            disabled: true
        }
    ],
    NORMAL: 'Color',
    WHITE: 'White',
    SCENE: 'Scene',
    GPIO_LIST: [
        {name: 'GPIO38', key: 38},
        {name: 'GPIO39', key: 39},
        {name: 'GPIO40', key: 40},
        {name: 'GPIO41', key: 41},
        {name: 'GPIO42', key: 42},
        {name: 'GPIO43', key: 43}
    ]
}
var Ajax = {
    get: function(url,callback){
        // XMLHttpRequest对象用于在后台与服务器交换数据
        var xhr=new XMLHttpRequest();
        xhr.open('GET', url,true);
        xhr.setRequestHeader('Content-Type','application/json');
        xhr.onreadystatechange=function(){
            if(xhr.readyState==4){
                if(xhr.status==200 || xhr.status==304){
                    console.log(xhr.responseText);
                    callback(xhr.responseText);
                } else {
                    console.log(xhr.responseText);
                }
            }
        }
        xhr.send();
    },
    put: function(url, data, callback) {
        var xhr=new XMLHttpRequest();
        xhr.open('put', url,true);
        xhr.setRequestHeader('Content-Type','application/json');
        xhr.onreadystatechange=function(){
            if(xhr.readyState==4){
                if(xhr.status==200 || xhr.status==304){
                    console.log(xhr.responseText);
                    callback(xhr.responseText);
                } else {
                    console.log(xhr.responseText);
                }
            }
        }
        xhr.send(JSON.stringify(data));
    },
    post: function(url, data, callback){
        var xhr=new XMLHttpRequest();
        xhr.open('POST', url,true);
        // 添加http头，发送信息至服务器时内容编码类型
        xhr.setRequestHeader('Content-Type','application/json');
        xhr.onreadystatechange=function(){
            if (xhr.readyState==4){
                if (xhr.status==200 || xhr.status==304){
                    callback(xhr.responseText);
                }
            }
        }
        xhr.send(data);
    }
}
function initNoData () {
    var dom = document.getElementById('no-data-info');
    var list = []
    CONSTANT.DEVICE_TYPE_LIST.forEach(function(item) {
        list.push(`<div class="img"><div class="${item.defaultIcon}"></div></div>`)
    })
    dom.innerHTML = list.join('')
}
function initAddDeviceType (id) {
    var dom = document.getElementById(id);
    var list = []
    CONSTANT.DEVICE_TYPE_LIST.forEach(function(item) {
        list.push(`<div onclick="addDeviceInfo('${item.defaultIcon}', ${item.disabled})" data-type="${item.defaultIcon}" class="item flex flex-ac ${item.disabled ? 'disabled' : ''}">
                   <div class="img ${item.defaultIcon}" ></div>
                    <div class="name">${item.label}</div>
                </div>`)
    })
    dom.innerHTML = list.join('')
}
function addDeviceInfo (type, disbaled) {
    if (disbaled) {
        return
    }
    var data = {'method': 'config', 'type': type, 'node_id': getUUID(), params: {config: {gpio: {red: 0, green: 1, blue: 2}, params: []}}}
    sessionStorage.setItem(CONSTANT.DEVICEINFO,JSON.stringify(data))
    selectHash(CONSTANT.PAGE_ADD_DEVICE_INFO)
}
function getDeviceList (callback) {
    Ajax.get(`${CONSTANT.NODES_INFO_URL}?time=${parseInt(new Date().getTime() / 1000).toString()}`, function(res) {
        console.log(res)
        callback(res)
    })
}
function controlDeviceStatus (item) {
    Ajax.put(CONSTANT.NODES_PARAMS_URL, {method: 'contrl', type: item.type, node_id: item.node_id, params: {status: item.params.status}}, function(res) {
        console.log(res)
    })
}
function getDeviceStatus (id) {
    var isOpen = false
    var deviceInfo = ''
    var list = sessionStorage.getItem(CONSTANT.DEVICELIST);
    if (list) {
        list = JSON.parse(list)
    } else {
        list = []
    }
    for (var i = 0; i < list.length; i++) {
        var item = list[i];
        if (item.node_id == id) {
            if (item.params.status) {
                item.params.status = 0
                isOpen = false
            } else {
                item.params.status = 1
                isOpen = true
            }
            deviceInfo = item
            list.splice(i, 1, item)
            break;
        }
    }
    sessionStorage.setItem(CONSTANT.DEVICELIST, JSON.stringify(list));
    return {isOpen: isOpen, deviceInfo: deviceInfo}
}
function getIcon (deviceInfo) {
    var info = {}
    for (var i = 0; i < CONSTANT.DEVICE_TYPE_LIST.length; i++) {
        var item = CONSTANT.DEVICE_TYPE_LIST[i]
        if (item.name.toLowerCase() == deviceInfo.type.toLowerCase()) {
            info = item
            break
        }
    }
    return info.defaultIcon || 'light'
}
function getUUID(len = 32, radix) {
    var chars = '0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz'.split('')
    var uuid = []
    var i
    radix = radix || chars.length
    if (len) {
        // Compact form
        for (i = 0; i < len; i++) uuid[i] = chars[0 | Math.random() * radix]
    } else {
        // rfc4122, version 4 form
        var r

        // rfc4122 requires these characters
        uuid[8] = uuid[13] = uuid[18] = uuid[23] = '-'
        uuid[14] = '4'

        // Fill in random data.  At i==19 set the high bits of clock sequence as
        // per rfc4122, sec. 4.1.5
        for (i = 0; i < 36; i++) {
            if (!uuid[i]) {
                r = 0 | Math.random() * 16
                uuid[i] = chars[(i === 19) ? (r & 0x3) | 0x8 : r]
            }
        }
    }
    return uuid.join('')
}
function hsv2rgb(h, s, v) {
    var i = Math.floor(h * 6)
    var f = h * 6 - i
    var [r, g, b, p, q, t] = [0, 0, 0, v * (1 - s), v * (1 - f * s), v * (1 - (1 - f) * s)]
    switch (i % 6) {
        case 0:
            [r, g, b] = [v, t, p]
            break
        case 1:
            [r, g, b] = [q, v, p]
            break
        case 2:
            [r, g, b] = [p, v, t]
            break
        case 3:
            [r, g, b] = [p, q, v]
            break
        case 4:
            [r, g, b] = [t, p, v]
            break
        case 5:
            [r, g, b] = [v, p, q]
            break
    }
    return [Math.floor(r * 255), Math.floor(g * 255), Math.floor(b * 255)]
}
function rgb2hsv(r, g, b) {
    r /= 255
    g /= 255
    b /= 255
    var [max, min] = [Math.max(r, g, b), Math.min(r, g, b)]
    var [h, s, l] = ['', '', (max + min) / 2]
    if (max === min) {
        h = s = 0 // achromatic
    } else {
        var d = max - min
        s = l > 0.5 ? d / (2 - max - min) : d / (max + min)
        switch (max) {
            case r:
                h = (g - b) / d + (g < b ? 6 : 0)
                break
            case g:
                h = (b - r) / d + 2
                break
            case b:
                h = (r - g) / d + 4
                break
        }
        h /= 6
    }
    return [Math.floor(h * 360), Math.floor(s * 100), Math.floor(l * 100)]
}
function initModel(id, list) {
    var domList = []
    list.forEach(function(item) {
        domList.push(`<li class="model-item" data-value="${item.value}">${item.label}</li>`)
    })
    document.getElementById(id).innerHTML = domList.join('')
}
function initSlider (id, value, changing, change) {
    var dom = document.getElementById(id)
    dom.innerHTML = `<div class="slider-wrap">
                    <div class="content">
                        <div class="bar">
                            <div data-value="0" class="progress"></div>
                            <div class="dot"></div>
                        </div>
                    </div>
                </div>`
    //  获取所有的节点元素
    var content = document.querySelector(`#${id} .content`)
    var bar = document.querySelector(`#${id} .bar`)
    var progress = document.querySelector(`#${id} .progress`)
    var dot = document.querySelector(`#${id} .dot`)
    var totalWidth = dom.clientWidth
    var rest = bar.offsetWidth - dot.offsetWidth
    dot.style.left = (totalWidth - dot.clientWidth ) * (value / 100) + 'px'
    progress.style.width = totalWidth * (value / 100)+ 'px';
    progress.setAttribute('data-value', value)
    // 鼠标按下事件
    dot.addEventListener('touchstart', function(ev) {
        console.log(ev)
        var dotL = dot.offsetLeft
        var e = ev.touches[0] || window.event.touches[0]
        var mouseX = e.clientX
        window.addEventListener('touchmove', move, { passive: false })

        window.addEventListener('touchend', function () {
            window.removeEventListener('touchmove', move, { passive: false })
        }, { passive: false })
        function move(ev) {
            var e = ev.touches[0] || window.event.touches[0]
            var moveL = e.clientX - mouseX
            var newL = dotL + moveL
            if (newL < 0) {
                newL = 0
            }
            if (newL >= rest) {
                newL = rest
            }
            dot.style.left = newL + 'px'
            var bili = newL / rest * 100
            progress.style.width = totalWidth * Math.ceil(bili)/100 + 'px';
            progress.setAttribute('data-value', Math.ceil(bili))
            changing(Math.ceil(bili))
        }
    }, { passive: false })
    bar.addEventListener('touchstart',function (ev) {
        ev = ev.touches[0] || window.event.touches[0]
        var left = ev.clientX - content.offsetLeft - dot.offsetWidth / 2
        if (left < 0) {
            left = 0
        }
        if (left >= rest) {
            left = rest
        }
        dot.style.left = left + 'px'
        var bili = left / rest * 100
        progress.style.width = totalWidth* Math.ceil(bili)/100 + 'px';
        progress.setAttribute('data-value', Math.ceil(bili))
        if (changing) {
            changing(Math.ceil(bili))
        }
    }, { passive: false })
    bar.addEventListener('touchend',function (ev) {
        if (change) {
            change()
        }
    }, { passive: false })
}
function setSelected (id, callback) {
    var liList = document.querySelectorAll('li.model-item');
    var val = document.getElementById(id).getAttribute('data-value');
    for (var i = 0; i < liList.length; i++) {
        var curDom = liList[i]
        var curVal = curDom.getAttribute('data-value')
        if (val == curVal) {
            curDom.classList.add('active');
            document.getElementById(id).innerText = curDom.innerText
        } else {
            curDom.classList.remove('active');
        }
    }
    if (val == 3) {
        document.querySelector('.command-color').classList.add('active')
        document.querySelector('.command-color-wrap').classList.add('active')
        if (callback) {
            callback()
        }
    } else {
        document.querySelector('.command-color').classList.remove('active')
        document.querySelector('.command-color-wrap').classList.remove('active')
    }
}
function getColor(hue, saturation, brightness) {
    return `${hsv2rgb(hue / 360, saturation / 100, 1).join(',')}, ${brightness / 100}`
}
function initDevcieVoice(params, id) {
    var list = []
    params.forEach(function(item, index) {
        var html = `<div onclick="editCommand(${index})" class="collapse-item flex flex-jcb flex-ac"><div class="collapse-item-info">`;
        if (item.status === 1 || item.status === 0) {
            html += `<div class="title">${item.status === 1 ? 'ON' : 'OFF'}</div>
                         <div class="desc">${item.zh || item.voice}</div>`
        } else {
            var rgba = getColor(item.hue, item.saturation, item.value)
            html += `<div class="flex flex-ac flex-jcb">
                            <div>
                             <div class="title">Color</div>
                             <div class="desc">${item.zh || item.voice}</div>
                            </div>
                            <span class="color" style="background: rgba(${rgba})"></span>
                            </div>`
        }
        html += `</div>
                        <div class="right-icon">
                            <i class="iconfont icon-right"></i>
                        </div>
                    </div>`
        list.push(html)
    })
    if (!list.length) {
        list.push(`<div class="no-data textCenter">No voice command, please add</div>`)
    }
    document.querySelector(`#${id} .collapse-content-add`).innerHTML = list.join('')
}
function saveDeviceData (deviceInfo, params) {
    if (params.length == 0) {
        toast('warning', 'Please add voice')
        return
    }
    var data = {'method': 'config', 'type': deviceInfo.type, 'node_id': deviceInfo.node_id, params: deviceInfo.params || {}}
    loading(this)
    Ajax.put(CONSTANT.NODES_PARAMS_URL, data, function() {
        console.log(res)
        clearLoading()
        if (res.status === 'success') {
            toast('suc', 'Success')
            setTimeout(() => {
                window.history.go(-2)
            }, 3000)
            return
        }
        toast('error', 'Filed')
    })
}
function toast(status, msg) {
    if (document.getElementById('toast-wrap')) {
        document.getElementById('toast-wrap').remove();
    }
    var div = document.createElement('div');
    div.id = 'toast-wrap'
    div.innerHTML = `<div class="toast-content ${status}">${msg}</div>`
    document.querySelector('body').appendChild(div)
    setTimeout(function() {
        if (document.getElementById('toast-wrap')) {
            document.getElementById('toast-wrap').remove();
        }
    }, 3000)
}
function loading () {
    var div = document.createElement('div');
    div.id = 'loading-wrap'
    div.innerHTML = `<div class="loading-content"><span class="van-loading__spinner van-loading__spinner--circular"><svg viewBox="25 25 50 50" class="van-loading__circular"><circle cx="50" cy="50" r="20" fill="none"></circle></svg></span></div>`
    document.querySelector('body').appendChild(div)
}
function clearLoading () {
    if (document.getElementById('loading-wrap')) {
        document.getElementById('loading-wrap').remove()
    }
}
function initCmudict () {
    var res = sessionStorage.getItem(CONSTANT.G2P_DATA_LIST)
    var device_type = navigator.userAgent
    var md = new MobileDetect(device_type); //初始化mobile-detect
    var os = md.os(); //获取系统
    console.log(os)
    if (!res) {
        JSZipUtils.getBinaryContent('js/cmudict.0.7a.zip', function(err, data) {
            if(err) {
                throw err;
            }
            JSZip.loadAsync(data, {optimizedBinaryString: true}).then(function (res) {
                //res.files里包含整个zip里的文件描述、目录描述列表
                //res本身就是JSZip的实例
                for (var key in res.files) {
                    res.file(res.files[key].name).async('string').then(content => {
                        console.log(content)
                        sessionStorage.setItem(CONSTANT.G2P_DATA_LIST, content)
                    })
                }
            }).catch(err => {
                console.log(err)
            });
        });
    }
}
function dowmGetDeviceList () {
    getDeviceList(function(res) {
        if (res) {
            res = JSON.parse(res)
            initDeviceItem(res.node_list || []);
        }
        mescroll.endErr();
    });
}
function initDeviceItem(list) {
    var itemList = []
    list = [{"type":"light","node_id":1,"params":{"status":0,"hue":170,"saturation":100,"value":30,"config":{"gpio":39,"params":[{"status":1,"voice":"da kai dian deng","zh":"打开电灯"},{"status":0,"voice":"guan bi dian deng","zh":"关闭电灯"}]}}}]
    list.forEach(function(item, index) {
        itemList.push(`<div onclick="showOperate(${index})" class="item flex-1"><div class="item-header flex flex-jcb flex-ac ${item.params.status ? 'active' : ''}"><div class="item-img"><div class="${getIcon(item)}"></div></div>
                    <div class="item-switch"><div onclick="deviceSwitch(this, ${item.node_id})" data-id="${item.node_id}" class="switch-btn ${item.params.status ? 'active' : ''}"></div></div>
                </div>
                    <div class="device-group-name item-name flex flex-ac flex-jcb flex-wrap">
                        <span class="flex-1">${item.type}</span>
                    </div>
                </div>`)
    })
    sessionStorage.setItem(CONSTANT.DEVICELIST, JSON.stringify(list));
    document.getElementById('item-content').innerHTML = itemList.join('');
    document.querySelector('.loading-wrap').classList.add('hide');
}
function deviceSwitch (ev, id) {
    window.event.stopPropagation();
    var obj = getDeviceStatus(id);
    var dom = ev.parentNode.parentNode;
    if (obj.isOpen) {
        dom.classList.add('active');
        ev.classList.add('active');
    } else {
        dom.classList.remove('active');
        ev.classList.remove('active');
    }
    controlDeviceStatus(obj.deviceInfo);
    return false;
}
function operateSwitch () {
    var deviceInfo = sessionStorage.getItem(CONSTANT.DEVICEINFO);
    if (deviceInfo) {
        deviceInfo = JSON.parse(deviceInfo);
        var obj = getDeviceStatus(deviceInfo.node_id);
        if (obj.isOpen) {
            document.querySelector('.device-control .power-wrap').classList.add('active');
        } else {
            document.querySelector('.device-control .power-wrap').classList.remove('active');
        }
        controlDeviceStatus(obj.deviceInfo);
    }
    return false
}
function selectHash (page) {
    window.location.hash = page;
    selectPage(page);
}
function showOperate(index) {
    var list = sessionStorage.getItem(CONSTANT.DEVICELIST);
    console.log(list);
    if (list) {
        list = JSON.parse(list);
        var item = list[index];
        sessionStorage.setItem(CONSTANT.DEVICEINFO, JSON.stringify(item));
        selectHash(CONSTANT.PAGE_OPERATE);
    }
}
function addDevice() {
    selectHash(CONSTANT.PAGE_ADD_DEVICE);
}
function selectPage (page) {
    var doms = document.getElementsByClassName('app-page');
    for (var i = 0; i < doms.length; i++) {
        var dom = doms[i];
        if (dom.getAttribute('id') == page) {
            dom.classList.add('show');
        } else {
            dom.classList.remove('show');
        }
    }
    if (page == CONSTANT.PAGE_HOME || page == '') {
        document.getElementById(CONSTANT.PAGE_HOME).classList.add('show');
    }
    initCase(page);
}
function pageBack (page) {
    if(page == CONSTANT.PAGE_HOME) {
        window.location.hash = '';
        selectPage(CONSTANT.PAGE_HOME);
    } else {
        window.location.hash = page;
        selectPage(page);
    }
}
function operateBack () {
    pageBack(CONSTANT.PAGE_HOME);
}
function initCase (page) {
    switch (page) {
        case '':
            initHome();
            break;
        case CONSTANT.PAGE_OPERATE:
            initOperate();
            break;
        case CONSTANT.PAGE_ADD_DEVICE:
            initAddDevice();
            break;
        case CONSTANT.PAGE_ADD_DEVICE_INFO:
            initAddDeviceInfo();
            break;
        case CONSTANT.PAGE_COMMAND:
            initCommand();
            break;
        default:
            initHome();
            break;
    }
}
function initHome () {
    initNoData();
    initDeviceItem([]);
    var devcieList = sessionStorage.getItem(CONSTANT.DEVICELIST);
    console.log(devcieList);
    if (devcieList) {
        initDeviceItem(JSON.parse(devcieList));
    } else {
        document.querySelector('.loading-wrap').classList.remove('hide');
    }
    var mescroll = new MeScroll("mescroll", {
        down: {
            callback: dowmGetDeviceList
        }
    });
}
var OPERATE = {
    hue: 0,
    brightness: 100,
    saturation: 100
}

function initOperate() {
    var deviceInfo = sessionStorage.getItem(CONSTANT.DEVICEINFO);
    console.log(deviceInfo);
    var params = [];
    document.querySelector('.tab-control').classList.remove('active');
    document.querySelector('.voice-config').classList.add('active');
    document.querySelector('.device-control').classList.remove('active');
    if (deviceInfo) {
        deviceInfo = JSON.parse(deviceInfo);
        var deviceParams = deviceInfo.params || {};
        params = deviceInfo.params.config.params || [];
        initDevcieVoice(params, CONSTANT.PAGE_OPERATE);
        OPERATE.hue = deviceParams.hue || 0;
        OPERATE.brightness = deviceParams.value || 100;
        OPERATE.saturation = deviceParams.saturation || 100;
        if (deviceParams.status == CONSTANT.ON_STATUS) {
            document.querySelector('.device-control .power-wrap').classList.add('active');
        } else {
            document.querySelector('.device-control .power-wrap').classList.remove('active');
        }
    }
    document.querySelector('.tab-wrap').addEventListener('click', function(ev) {
        console.log(ev.target.classList);
        if (ev.target.classList.contains('tab-item')) {
            ev.target.classList.add('active');
        }
        if (ev.target.classList.contains('tab-voice')) {
            document.querySelector('.tab-control').classList.remove('active');
            document.querySelector('.voice-config').classList.add('active');
            document.querySelector('.device-control').classList.remove('active');
        } else if (ev.target.classList.contains('tab-control')) {
            document.querySelector('.tab-voice').classList.remove('active');
            document.querySelector('.device-control').classList.add('active');
            document.querySelector('.voice-config').classList.remove('active');
            startInitOperateSlider();
        }
    })


}
function startInitOperateSlider () {
    initSlider('color-slider', OPERATE.hue / 3.6, function(val) {
        OPERATE.hue = val * 3.6;
    }, function(val) {
        changeDeviceControl();
    })
    initSlider('brightness-slider', OPERATE.brightness, function(val) {
        OPERATE.brightness = val;
    }, function(val) {
        changeDeviceControl();
    })
    initSlider('saturation-slider', OPERATE.saturation, function(val) {
        OPERATE.saturation = val;
    }, function(val) {
        changeDeviceControl();
    })
}
function changeDeviceControl() {
    var deviceInfo = sessionStorage.getItem(CONSTANT.DEVICEINFO);
    if (deviceInfo) {
        deviceInfo = JSON.parse(deviceInfo);
    }
    var data = {'method': 'contrl', 'type': deviceInfo.type, 'node_id': deviceInfo.node_id, 'params': {hue: OPERATE.hue, saturation: OPERATE.saturation, value: OPERATE.brightness}};
    console.log(data);
    Ajax.put(CONSTANT.NODES_PARAMS_URL, data, function(res) {
        console.log(res);
    })
}
function saveOperateData () {
    var deviceInfo = sessionStorage.getItem(CONSTANT.DEVICEINFO);
    var params = [];
    if (deviceInfo) {
        deviceInfo = JSON.parse(deviceInfo);
        var deviceParams = deviceInfo.params || [];
        params = deviceInfo.params.config.params || [];
        saveDeviceData(deviceInfo, params);
    }
}
function initAddDevice () {
    initAddDeviceType('device-type-list');
}
var COMMAND = {
    list: [{label: 'ON', value: 1}, {label: 'OFF', value: 0}, {label: 'Color', value: 3}, {label: 'Cancel', value: 'cancel'}],
    hue: 0,
    brightness: 100,
    saturation: 100,
    commandVal: 1,
    voiceVal: ''
}
function initCommand () {
    console.log(COMMAND);
    COMMAND.hue = 0;
    COMMAND.brightness = 100;
    COMMAND.saturation = 100;
    COMMAND.commandVal = '1';
    COMMAND.voiceVal = '';
    var deviceInfo = sessionStorage.getItem(CONSTANT.DEVICEINFO);
    var commandInfo = sessionStorage.getItem(CONSTANT.COMMANDINFO);
    console.log(commandInfo);
    if (deviceInfo) {
        deviceInfo = JSON.parse(deviceInfo);
    }
    if (commandInfo) {
        commandInfo = JSON.parse(commandInfo);
        if (commandInfo.hue != null || commandInfo.saturation != null) {
            COMMAND.hue = commandInfo.hue;
            COMMAND.brightness = commandInfo.value;
            COMMAND.saturation = commandInfo.saturation;
            COMMAND.commandVal = '3';
            document.getElementById('command-info').setAttribute('data-value', COMMAND.commandVal);
            changeColor();
            setSelected('command-info', startInitCommandSlider);
        } else {
            COMMAND.commandVal = commandInfo.status;
            document.getElementById('command-info').setAttribute('data-value', COMMAND.commandVal);
            setSelected('command-info');
        }
        COMMAND.voiceVal = commandInfo.zh || commandInfo.voice;
        document.getElementById('voice').value = COMMAND.voiceVal;
        document.querySelector('.word-count-info').innerText = COMMAND.voiceVal.length;
    } else {
        document.getElementById('command-info').setAttribute('data-value', COMMAND.commandVal);
        document.getElementById('voice').value = COMMAND.voiceVal;
        setSelected('command-info');
    }

    initModel('model-content-info', COMMAND.list);
    setTimeout(function() {
        setSelected('command-info');
    }, 100)
    document.getElementById('model-content-info').addEventListener('click', function(ev) {
        ev.preventDefault();
        ev.stopPropagation();
        var dom = ev.target || ev.srcElement;
        if (dom.classList.contains('model-item')) {
            var val = dom.getAttribute('data-value');
            if (val != 'cancel') {
                document.getElementById('command-info').setAttribute('data-value', val);
                COMMAND.commandVal = val;
                setSelected('command-info', startInitCommandSlider);
            }
            document.querySelector('.model-wrap').classList.remove('active');
        }
    })
    document.getElementById('select-command').onclick = function() {
        console.log('ssssa');
        var domModel = document.querySelector('.model-wrap');
        if (domModel.classList.contains('active')) {
            domModel.classList.remove('active');
        } else {
            domModel.classList.add('active');
        }
        console.log(domModel.classList);
    }
    document.getElementById('command-color').onclick = function() {
        var dom = document.querySelector('.command-color-wrap');
        if (dom.classList.contains('active')) {
            dom.classList.remove('active')
        } else {
            dom.classList.add('active')
            startInitCommandSlider()
        }
    }
    document.querySelector('.model-wrap').onclick = function() {
        this.classList.remove('active')
    }
    document.getElementById('voice').oninput = function() {
        COMMAND.voiceVal = this.value
        document.querySelector('.word-count-info').innerText = this.value.length
    }
}
function startInitCommandSlider () {
    initSlider('command-color-slider', COMMAND.hue / 3.6, function(val) {
        COMMAND.hue = val * 3.6;
        changeColor()
    })
    initSlider('command-brightness-slider', COMMAND.brightness, function(val) {
        COMMAND.brightness = val
        changeColor()
    })
    initSlider('command-saturation-slider', COMMAND.saturation, function(val) {
        COMMAND.saturation = val
        changeColor()
    })
}
function changeColor () {
    var rgba = `${hsv2rgb(COMMAND.hue / 360, COMMAND.saturation / 100, 1).join(',')}, ${COMMAND.brightness / 100}`;
    document.getElementById('command-color').style.backgroundColor = `rgba(${rgba})`
}
function saveCommand () {
    var deviceInfo = sessionStorage.getItem(CONSTANT.DEVICEINFO)
    if (deviceInfo) {
        deviceInfo = JSON.parse(deviceInfo)
    }
    var commandInfo = {}
    if (!COMMAND.voiceVal) {
        toast('warning', 'Please enter voice')
        return
    }
    console.log(COMMAND)
    if (COMMAND.commandVal == 3) {
        commandInfo.hue = parseInt(COMMAND.hue)
        commandInfo.value = parseInt(COMMAND.brightness)
        commandInfo.saturation = parseInt(COMMAND.saturation)
    } else {
        commandInfo.status = parseInt(COMMAND.commandVal)
    }
    var voice = ''
    if (CheckChinese(COMMAND.voiceVal)) {
        voice = Pinyin.getFullChars(COMMAND.voiceVal).split(' ')
        voice = voice.map(item => {
            return item.toLowerCase()
        })
    } else {
        var r = g2p()
        voice = r.pronounce(COMMAND.voiceVal)
    }
    commandInfo.voice = voice.join(' ').trim()
    commandInfo.zh = COMMAND.voiceVal
    console.log(commandInfo)
    var index = sessionStorage.getItem(CONSTANT.COMMANDINDEX)
    var params = deviceInfo.params.config.params || []
    if (index) {
        index = parseInt(index)
    } else {
        index = -1
    }
    if (index > -1) {
        params.splice(index, 1, commandInfo)
    } else {
        params.push(commandInfo)
    }
    deviceInfo.params.config.params = params
    var list = sessionStorage.getItem(CONSTANT.DEVICELIST)
    if (list) {
        list = JSON.parse(list)
    } else {
        list = []
    }
    for (var i = 0; i < list.length; i++) {
        var item = list[i]
        if (item.node_id === deviceInfo.node_id) {
            list.splice(i, 1, deviceInfo)
            sessionStorage.setItem(CONSTANT.DEVICELIST, JSON.stringify(list))
            break
        }
    }
    sessionStorage.setItem(CONSTANT.DEVICEINFO, JSON.stringify(deviceInfo))
    goBack()
}

function initAddDeviceInfo () {
    var deviceInfo = sessionStorage.getItem(CONSTANT.DEVICEINFO)
    var params = []
    if (deviceInfo) {
        deviceInfo = JSON.parse(deviceInfo)
        params = deviceInfo.params.config.params || []
        initDevcieVoice(params, CONSTANT.PAGE_ADD_DEVICE_INFO)
    }
}
function editCommand (index) {
    var deviceInfo = sessionStorage.getItem(CONSTANT.DEVICEINFO)
    var params = []
    if (deviceInfo) {
        deviceInfo = JSON.parse(deviceInfo)
        params = deviceInfo.params.config.params || []
        sessionStorage.setItem(CONSTANT.COMMANDINFO, JSON.stringify(params[index]))
        sessionStorage.setItem(CONSTANT.COMMANDINDEX, index)
        selectHash(CONSTANT.PAGE_COMMAND)
    }
}
function addCommand () {
    sessionStorage.setItem(CONSTANT.COMMANDINFO, '')
    sessionStorage.setItem(CONSTANT.COMMANDINDEX, '')
    selectHash(CONSTANT.PAGE_COMMAND)
}
function saveDeviceInfoData () {
    var deviceInfo = sessionStorage.getItem(CONSTANT.DEVICEINFO)
    var params = []
    if (deviceInfo) {
        deviceInfo = JSON.parse(deviceInfo)
        params = deviceInfo.params.config.params || []
    }
    saveDeviceData(deviceInfo, params)
}
function CheckChinese (val) {
    var reg = new RegExp('[\\u4E00-\\u9FFF]+', 'g')
    return reg.test(val)
}
function initHash () {
    var hash = window.location.hash
    if (hash.indexOf('#') != -1) {
        hash = hash.split('#')[1]
    }
    selectPage(hash)
}
function goBack() {
    window.history.back();
}
