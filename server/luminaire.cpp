#include "luminaire.hpp"

luminaire::luminaire(int last_desk_, int samples_holder)
    : last_desk(last_desk_), desksDB(last_desk + 1)
    , cv(last_desk + 1), stream_mtx(last_desk + 1)
{
    for(int i = 0; i <= last_desk; i ++)
        desksDB[i] = std::make_shared<deskDB> (samples_holder);

    if(gpioInitialise()<0)
        printf("\nFailed to initialize gpio");
    else
        status = init_slave(xfer, SLAVE_ADDR_DEFAULT);
}

luminaire::~luminaire(){
    close_slave(xfer);
    gpioTerminate();
}

int luminaire::init_slave(bsc_xfer_t &xfer, int addr) {
    gpioSetMode(18, PI_ALT3);
    gpioSetMode(19, PI_ALT3);
    xfer.control = (addr<<16) |/* Slave address */
    (0x00<<13) | /* invert transmit status flags */
    (0x00<<12) | /* enable host control */
    (0x00<<11) | /* enable test fifo */
    (0x00<<10) | /* invert receive status flags */
    (0x01<<9) | /* enable receive */
    (0x01<<8) | /* enable transmit */
    (0x00<<7) | /* abort and clear FIFOs */
    (0x00<<6) | /* send control reg as 1st I2C byte */
    (0x00<<5) | /* send status regr as 1st I2C byte */
    (0x00<<4) | /* set SPI polarity high */
    (0x00<<3) | /* set SPI phase high */
    (0x01<<2) | /* enable I2C mode */
    (0x00<<1) | /* enable SPI mode */
    0x01 ; /* enable BSC peripheral */
    return bscXfer(&xfer);
}

void luminaire::close_slave(bsc_xfer_t &xfer){
    xfer.control=0;
    bscXfer(&xfer);
}

void luminaire::make_read(int first){
    if ((char) xfer.rxBuf[first] == 'r')
        read_reaclib(first);
    else if((char) xfer.rxBuf[first] == 's')
        read_sample(first);
}

void luminaire::read_reaclib(int first){
    int desk  = (int) xfer.rxBuf[first + 1];
    float lux = (int) xfer.rxBuf[first + 2] + 0.01 * (int) xfer.rxBuf[first + 3];
    float duty_cycle = (int) xfer.rxBuf[first + 4] + 0.01 * (int) xfer.rxBuf[first + 5];
    bool occupancy = (bool) xfer.rxBuf[first + 6];
    float control_ref = (int) xfer.rxBuf[first + 7] + 0.01 * (int) xfer.rxBuf[first + 8];

    insert_sample(desk, lux, duty_cycle, occupancy, control_ref);

    if (xfer.rxCnt > first + 9)
        make_read(first + 9);
}

void luminaire::read_sample(int first){
    int desk  = (int) xfer.rxBuf[first + 1];
    float lower_bound_off = (int) xfer.rxBuf[first + 2] + 0.01 * (int) xfer.rxBuf[first + 3];
    float lower_bound_on = (int) xfer.rxBuf[first + 4] + 0.01 * (int) xfer.rxBuf[first + 5];
    float ext_lux = (int) xfer.rxBuf[first + 6] + 0.01 * (int) xfer.rxBuf[first + 7];

    set_parameters(desk, lower_bound_off, lower_bound_on, ext_lux);

    if (xfer.rxCnt > first + 8)
        make_read(first + 8);
}

void luminaire::read_data(bool& server_up){
    while (server_up){
       // usleep(500);
        mtx.lock();

            xfer.txCnt = 0;
            status = bscXfer(&xfer);
            if (status < 0){
                std::cout << "\nbscXfer() returned negative status.\n";
                break;
            }
            if (xfer.rxCnt > 0){
                //printf("\nReceived %d bytes\n", xfer.rxCnt);
                make_read(0);
                /*
                printf("\nReceived %d bytes\n", xfer.rxCnt);

                for(int j=0;j<xfer.rxCnt;j++)
                printf("%d", (int) xfer.rxBuf[first + j]);*/
            }
        
        mtx.unlock();
    }
}

void luminaire::stop_read(){
    if (read_state){
        mtx.lock();
        read_state = false;
    }
}

void luminaire::resume_read(){
    if(!read_state){
        mtx.unlock();
        read_state = true;
    }

}

void luminaire::change_slave_addr(int addr){
    if (! read_state){
        throw std::invalid_argument("\nCan't change the slave address if luminaire is stopped."); 
        return;
    }
    mtx.lock();
    status = init_slave(xfer, addr);
    if (status < 0){
        throw std::invalid_argument("\nAddress change failed, default address reset."); 
        status = init_slave(xfer, addr);
    }
    mtx.unlock();
}



/************************
        Desks Access
*************************/
float luminaire::get_lux(int desk){
    if (desk > last_desk)
        return-1;

    return desksDB[desk]->get_lux();
}

int ret_cvwait;
float luminaire::get_lux_on_change(int desk){
    if (desk > last_desk)
        return-1;
    
    std::unique_lock<std::mutex> lock(stream_mtx[desk]);
    if (cv[desk].wait_for(lock, std::chrono::milliseconds(30)), []{return ret_cvwait == 1;})
        return desksDB[desk]->get_lux();
    else //timed out
        return -1;
}

float luminaire::get_duty_cycle(int desk){
    if (desk > last_desk)
        return-1;
    
    return desksDB[desk]->get_duty_cycle();
}

float luminaire::get_duty_cycle_on_change(int desk){
    if (desk > last_desk)
        return-1;
    
    std::unique_lock<std::mutex> lock(stream_mtx[desk]);
    if (cv[desk].wait_for(lock, std::chrono::milliseconds(30)), []{return ret_cvwait == 1;})
        return desksDB[desk]->get_duty_cycle();
    else //timed out
        return -1;
}

bool luminaire::get_occupancy(int desk){
    if (desk > last_desk)
        return false;
    
    return desksDB[desk]->get_occupancy();
}

float luminaire::get_lower_bound(int desk){
    if (desk > last_desk)
        return-1;

    return desksDB[desk]->get_lower_bound();
}

float luminaire::get_ext_lux(int desk){
    if (desk > last_desk)
        return-1;

    return desksDB[desk]->get_ext_lux();
}

float luminaire::get_control_ref(int desk){
    if (desk > last_desk)
        return-1;

    return desksDB[desk]->get_control_ref();
}

float luminaire::get_power(int desk){
    if (desk > last_desk)
        return-1;

    return desksDB[desk]->get_power();
}

float luminaire::get_restart_time(int desk){
    if (desk > last_desk)
        return-1;

    return desksDB[desk]->get_restart_time();
}

float luminaire::get_energy(int desk){
    if (desk > last_desk)
        return-1;

    return desksDB[desk]->get_energy();
}

float luminaire::get_comfort(int desk){
    if (desk > last_desk)
        return-1;

    return desksDB[desk]->get_comfort();
}

float luminaire::get_comfort_flicker(int desk){
    if (desk > last_desk)
        return-1;

    return desksDB[desk]->get_comfort_flicker();
}

void luminaire::get_lux_holder(int desk, std::vector<float>& holder){
    if (desk > last_desk)
        return;

    desksDB[desk]->get_lux_holder(holder);
}

void luminaire::get_duty_cycle_holder(int desk, std::vector<float>& holder){
    if (desk > last_desk)
        return;
    
    desksDB[desk]->get_duty_cycle_holder(holder);
}

void luminaire::insert_sample(int desk, float lux, float duty_cycle, bool occupancy, float control_ref){
    if (desk > last_desk)
        return;

    desksDB[desk]->insert_sample(lux, duty_cycle, occupancy, control_ref);
    cv[desk].notify_all();
}

void luminaire::set_parameters(int desk, float lower_bound_off, float lower_bound_on, float ext_lux){
    if (desk > last_desk)
        return;

    desksDB[desk]->set_parameters(lower_bound_off, lower_bound_on, ext_lux);
}

void luminaire::clear_desk(int desk){
    if (desk > last_desk)
        return;

    desksDB[desk]->clearDB();
}
