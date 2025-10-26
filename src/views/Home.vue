<template>
  <div class="container">
    <h1 @click="handleClick">这是一个test的标题</h1>
    <p>这是一个段落，<a href="https://www.baidu.com/">这是一个链接</a></p>
    <ul>
      <li>列表项 1</li>
      <li>列表项 2</li>
    </ul>
  </div>
</template>

<script setup>
const handleClick = () => {
  alert("点击了哦~");
};
</script>

<style scoped>
.container {
  justify-items: center;
}

h1 {
  cursor: pointer;
}
</style>