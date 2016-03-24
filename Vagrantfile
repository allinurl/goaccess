$prepare = <<SCRIPT
# repo for libgeoip
add-apt-repository -y ppa:maxmind/ppa
apt-get update
apt-get install -y git autoconf libncursesw5-dev libgeoip-dev
echo "cd /vagrant && autoreconf -fiv && ./configure --enable-geoip --enable-utf8 && make && make install" > /usr/local/bin/build-goaccess
chmod +x /usr/local/bin/build-goaccess
# create command to build goaccess
build-goaccess
goaccess --version
SCRIPT

Vagrant.configure(2) do |config|
  config.vm.box = "ubuntu/trusty64"

  config.vm.provision "shell", inline: $prepare

  config.vm.network "forwarded_port", guest: 80, host: 8888

  config.vm.provider "virtualbox" do |v|
    v.name   = "goaccess"
    v.memory = 2048
  end
end