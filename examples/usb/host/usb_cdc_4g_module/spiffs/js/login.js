const baseurl = 'http://192.168.4.1'
const baseurl = window.location.origin
const CONSTANT = {
    POST_LOGIN_URL: `${baseurl}/login`,
}

var Ajax = {
    post: function(url, data, callback){
        var xhr=new XMLHttpRequest();
        xhr.open('POST', url,true);
        // 添加http头，发送信息至服务器时内容编码类型
        xhr.setRequestHeader('Content-Type','application/json');
        xhr.onreadystatechange=function(){
            console.log('1: ', xhr.readyState)
            console.log('2: ', xhr.status)
            console.log('3: ', xhr.responseText)
            if (xhr.readyState === 4){
                if (xhr.status === 200 || xhr.status === 304){
                    callback(xhr.responseText);
                }
            }
        }
        xhr.send(JSON.stringify(data));
    }
}

function login() {
    var wlanName = document.getElementById('wlanName')
    console.log('wlan 名称：', wlanName.value)
    var model = document.getElementById('model')
    console.log('安全模式：', model.value)
    var password = document.getElementById('password')
    console.log('密码：', password.value)
    var select = document.getElementById('select')
    console.log('wlan 隐身：', select.checked)
    var isSelect = 'false'
    if (select.checked) {
        isSelect = 'true'
    }
    Ajax.post(CONSTANT.POST_LOGIN_URL, {ssid: wlanName.value, if_hide_ssid: isSelect, auth_mode: model.value, password: password.value}, function (res) {
        console.log('基本信息保存：', res)
    })
}
