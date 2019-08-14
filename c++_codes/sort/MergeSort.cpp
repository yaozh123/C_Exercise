#include<iostream>
using namespace std;

void print(int a[], int n){
    for(int i=0; i<n; i++){
        cout<<a[i] <<" ";
    }
    cout<<endl;
}

void merge(int data[], int result[], int start, int end){
    int left_length=(end-start+1)/2+1;
    int left_index=start;
    int right_index=start+left_length;
    int result_index=start;
    while(left_index<start+left_length && right_index<end+1){
        if(data[left_index]<=data[right_index])
            result[result_index++]=data[left_index++];
        else
            result[result_index++]=data[right_index++];
    }
    while(left_index<start+left_length)
        result[result_index++]=data[left_index++];
    while(right_index<end+1)
        result[result_index++]=data[right_index++];
}

void MergeSort(int data[], int result[], int start, int end){
    if(start==end) return;
    else if(end-start==1){
        if(data[start]>data[end]){
            swap(data[start], data[end]);
        }
        return;
    }
    else{
        int middle=(end-start+1)/2+start;
        MergeSort(data, result, start, middle);
        MergeSort(data, result, middle+1, end);
        merge(data, result, start, end);
        for(int i=start; i<=end; i++)
            data[i]=result[i];
    }
}

int main(){
    int s[10]={2,5,1,9,6,0,3,9,2,4};
    int result[10];
    cout<<"初始序列：";
    print(s,10);
    MergeSort(s, result, 0, 9);
    cout<<"排序后序列：";
    print(s,10);
}
