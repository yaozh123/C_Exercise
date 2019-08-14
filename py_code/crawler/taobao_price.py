#淘宝商品比价实例
#目标：获取淘宝搜索页面的信息，提取其中的商品名称和价格
#步骤一：提交商品搜索请求，循环获取页面
#步骤二：对于每个页面，提取商品名称和价格信息。
#步骤三：将信息输出到屏幕上

import requests
import re

def getHTMLText(url):
    try:
        r = requests.get(url)
        r.raise_for_status()
        r.encoding = r.apparent_encoding
        return r.text
    except:
        return ""

def parsePage(ilt, html):
    try:
        plt = re.findall(r'\"view_price\"\:\"[\d\.]*\"', html)
        tlt = re.findall(r'\"raw_title\"\:\".*?\"', html)
        for i in range(len(plt)):
            price = eval(plt[i].split(':')[1])
            title = eval(tlt[i].split(':')[1])
            ilt.append([price, title])
    except:
        print("")


def printGoodsList(ilt):
    tplt = "{:4}\t{:8}\t{:16}"
    print(tplt.format("序号", "价格", "商品名称"))
    count = 0
    for g in ilt:
        count = count + 1
        print(tplt.format(count, g[0], g[1]))

def main():
    goods = "衣服"
    depth = 2       #表示爬取第一页和第二页
    start_url = 'https://s.taobao.com/search?q=' + goods
    
    infoList = [] 
    for i in range(depth):
        try:
            url = start_url + '&s=' + str(44*i)
            html = getHTMLText(url)
            parsePage(infoList, html)
        except:
            continue
    printGoodsList(infoList)
    '''
    ilt = []
    url = start_url + '&s=0'
    html = getHTMLText(url)
    plt = re.findall(r'\"view_price\"\:\"[\d\.]*\"', html)
    tlt = re.findall(r'\"raw_title\"\:\".*?\"', html)
    for i in range(len(plt)):
        price = eval(plt[i].split(':')[1])
        title = eval(tlt[i].split(':')[1])
        ilt.append([price, title])
    tplt = "{:4}\t{:8}\t{:16}"
    print(tplt.format("序号", "价格", "商品名称"))
    count = 0
    for g in ilt:
        count = count + 1
        print(tplt.format(count, g[0], g[1]))
    '''
    

main()

